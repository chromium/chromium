// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/uia_element_probe_impl.h"

#include <objbase.h>

#include <windows.h>

#include <oleauto.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_variant.h"
#include "chrome/services/util_win/public/cpp/uia_element_probe_types.h"
#include "chrome/services/util_win/uia_window_identity.h"
#include "ui/gfx/geometry/rect.h"

#include <uiautomation.h>

using Microsoft::WRL::ComPtr;

namespace {

bool IsWordStart(std::wstring_view id, size_t pos) {
  if (pos == 0 || !base::IsAsciiAlpha(id[pos - 1])) {
    return true;
  }

  // Treats 'B' in "fooBar" and 'S' in "UISet" as word starts.
  return base::IsAsciiUpper(id[pos]) &&
         (base::IsAsciiLower(id[pos - 1]) ||
          (pos + 1 < id.size() && base::IsAsciiLower(id[pos + 1])));
}

}  // namespace

namespace uia_element_probe_internal {

bool ContainsFragmentAtWordStart(std::wstring_view automation_id,
                                 std::wstring_view fragment) {
  if (fragment.empty() || fragment.size() > automation_id.size()) {
    return false;
  }

  for (size_t pos = 0; pos + fragment.size() <= automation_id.size(); ++pos) {
    if (IsWordStart(automation_id, pos) &&
        base::EqualsCaseInsensitiveASCII(
            automation_id.substr(pos, fragment.size()), fragment)) {
      return true;
    }
  }

  return false;
}

}  // namespace uia_element_probe_internal

namespace {

using uia_element_probe_internal::ContainsFragmentAtWordStart;

// Per-call UIA timeout, set above the caller's deadline so the caller times out
// first.
constexpr int kUiaTimeoutMs = 3000;

bool CachedIdHasFragment(IUIAutomationElement* element,
                         std::wstring_view fragment) {
  base::win::ScopedVariant value;
  if (FAILED(element->GetCachedPropertyValue(UIA_AutomationIdPropertyId,
                                             value.Receive())) ||
      value.type() != VT_BSTR || !V_BSTR(value.ptr())) {
    return false;
  }
  const BSTR id = V_BSTR(value.ptr());
  // SAFETY: A non-null BSTR points to ::SysStringLen(id) valid wchar_t
  // elements.
  return ContainsFragmentAtWordStart(
      base::as_string_view(UNSAFE_BUFFERS(base::span(id, ::SysStringLen(id)))),
      fragment);
}

// Returns true if `element` is cached as off-screen, or if its cached value is
// missing or not VARIANT_FALSE.
bool CachedIsOffscreen(IUIAutomationElement* element) {
  base::win::ScopedVariant value;
  if (FAILED(element->GetCachedPropertyValue(UIA_IsOffscreenPropertyId,
                                             value.Receive()))) {
    return true;
  }
  return value.type() != VT_BOOL || V_BOOL(value.ptr()) != VARIANT_FALSE;
}

gfx::Rect CachedBounds(IUIAutomationElement* element) {
  RECT bounds;
  if (FAILED(element->get_CachedBoundingRectangle(&bounds))) {
    return gfx::Rect();
  }
  return gfx::Rect(bounds);
}

// Returns `element`'s cached bounds if on-screen and centered within
// `window_bounds`, or an empty rect otherwise.
gfx::Rect UsableBoundsInWindow(IUIAutomationElement* element,
                               const gfx::Rect& window_bounds) {
  if (CachedIsOffscreen(element)) {
    return gfx::Rect();
  }
  const gfx::Rect bounds = CachedBounds(element);
  if (bounds.IsEmpty() || !window_bounds.Contains(bounds.CenterPoint())) {
    return gfx::Rect();
  }
  return bounds;
}

// Returns a cache request capturing the automation ID, is-offscreen, and
// bounding rectangle properties of all elements (via the raw view, not just
// controls), without retaining live element references.
ComPtr<IUIAutomationCacheRequest> MakeCacheRequest(IUIAutomation* uia) {
  ComPtr<IUIAutomationCacheRequest> request;
  if (FAILED(uia->CreateCacheRequest(&request))) {
    return nullptr;
  }

  for (PROPERTYID property :
       {UIA_AutomationIdPropertyId, UIA_IsOffscreenPropertyId,
        UIA_BoundingRectanglePropertyId}) {
    if (FAILED(request->AddProperty(property))) {
      return nullptr;
    }
  }

  ComPtr<IUIAutomationCondition> raw_view;
  if (FAILED(request->put_AutomationElementMode(AutomationElementMode_None)) ||
      FAILED(uia->get_RawViewCondition(&raw_view)) ||
      FAILED(request->put_TreeFilter(raw_view.Get()))) {
    return nullptr;
  }

  return request;
}

// Returns the bounds of the first element under `root`, in tree order, that
// matches `condition`, passes `accept`, and is usable inside `window_bounds`
// (see UsableBoundsInWindow()), or an empty rect if none is. `cache` must be a
// MakeCacheRequest() request, as `accept` and the usability check read only
// cached properties.
gfx::Rect FindFirstUsable(
    IUIAutomationElement* root,
    IUIAutomationCondition* condition,
    IUIAutomationCacheRequest* cache,
    const gfx::Rect& window_bounds,
    base::FunctionRef<bool(IUIAutomationElement*)> accept) {
  ComPtr<IUIAutomationElementArray> found;
  if (FAILED(root->FindAllBuildCache(TreeScope_Descendants, condition, cache,
                                     &found)) ||
      !found) {
    return gfx::Rect();
  }

  int count = 0;
  if (FAILED(found->get_Length(&count))) {
    return gfx::Rect();
  }

  for (int i = 0; i < count; ++i) {
    ComPtr<IUIAutomationElement> element;
    if (FAILED(found->GetElement(i, &element)) || !element ||
        !accept(element.Get())) {
      continue;
    }

    if (const gfx::Rect bounds =
            UsableBoundsInWindow(element.Get(), window_bounds);
        !bounds.IsEmpty()) {
      return bounds;
    }
  }

  return gfx::Rect();
}

gfx::Rect FindByExactId(IUIAutomation* uia,
                        IUIAutomationElement* root,
                        IUIAutomationCacheRequest* cache,
                        const gfx::Rect& window_bounds,
                        const std::wstring& automation_id) {
  base::win::ScopedVariant id(automation_id.c_str());
  ComPtr<IUIAutomationCondition> condition;
  if (FAILED(uia->CreatePropertyCondition(UIA_AutomationIdPropertyId, id,
                                          &condition))) {
    return gfx::Rect();
  }

  return FindFirstUsable(root, condition.Get(), cache, window_bounds,
                         [](IUIAutomationElement*) { return true; });
}

gfx::Rect FindByIdFragment(IUIAutomation* uia,
                           IUIAutomationElement* root,
                           IUIAutomationCacheRequest* cache,
                           const gfx::Rect& window_bounds,
                           std::wstring_view fragment,
                           int control_type) {
  ComPtr<IUIAutomationCondition> condition;
  if (control_type) {
    base::win::ScopedVariant type(
        static_cast<long>(control_type));  // NOLINT(runtime/int)
    if (FAILED(uia->CreatePropertyCondition(UIA_ControlTypePropertyId, type,
                                            &condition))) {
      return gfx::Rect();
    }
  } else if (FAILED(uia->CreateTrueCondition(&condition))) {
    return gfx::Rect();
  }

  return FindFirstUsable(root, condition.Get(), cache, window_bounds,
                         [fragment](IUIAutomationElement* element) {
                           return CachedIdHasFragment(element, fragment);
                         });
}

// Returns the bounds of a usable element matching `query` and of `hwnd`, or
// nullopt if none is found.
//
// Queries the target window with at most two FindAllBuildCache() calls over
// the raw subtree, fetching AutomationId, IsOffscreen, and BoundingRectangle
// in bulk to avoid per-element cross-process round trips:
// 1. Exact match on `query.automation_id`. Uses FindAll rather than FindFirst
//    so an earlier off-screen or out-of-window match cannot hide a usable one.
// 2. Fallback match when (1) finds nothing usable. Filters in UIA by
//    `query.fallback_control_type` (which callers should set to avoid
//    returning every element in the window) and matches
//    `query.fallback_automation_id_fragment` locally against cached IDs.
//
// Called once up front by FindElement() (e.g. from UiaElementLocatorWin for
// the Windows Settings "Set default" button). When waiting, re-runs at most
// once per `kChangeSettleDelay` after UIA structure-changed events and every
// `kFallbackPollInterval` until found, `hwnd` fails validation, a newer call
// supersedes it, or `query.wait_for` expires.
std::optional<UiaElementMeasurement> Search(IUIAutomation* uia,
                                            IUIAutomationElement* root,
                                            HWND hwnd,
                                            const UiaElementQuery& query) {
  RECT window_rect;
  if (!::GetWindowRect(hwnd, &window_rect)) {
    return std::nullopt;
  }

  const gfx::Rect window_bounds(window_rect);
  if (window_bounds.IsEmpty()) {
    return std::nullopt;
  }

  ComPtr<IUIAutomationCacheRequest> cache = MakeCacheRequest(uia);
  if (!cache) {
    return std::nullopt;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  gfx::Rect bounds;
  if (!query.automation_id.empty()) {
    bounds = FindByExactId(uia, root, cache.Get(), window_bounds,
                           query.automation_id);
  }

  if (bounds.IsEmpty() && !query.fallback_automation_id_fragment.empty()) {
    bounds = FindByIdFragment(uia, root, cache.Get(), window_bounds,
                              query.fallback_automation_id_fragment,
                              query.fallback_control_type);
  }

  if (bounds.IsEmpty()) {
    return std::nullopt;
  }

  return UiaElementMeasurement{bounds, window_bounds};
}

// Coalesces UIA structure-changed callbacks (invoked on UIA threads) so at
// most one notification is in flight until rearmed before the next search.
class StructureChangedHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUIAutomationStructureChangedEventHandler> {
 public:
  // `on_changed` may be invoked on arbitrary UIA threads.
  explicit StructureChangedHandler(base::RepeatingClosure on_changed)
      : on_changed_(std::move(on_changed)) {}

  StructureChangedHandler(const StructureChangedHandler&) = delete;
  StructureChangedHandler& operator=(const StructureChangedHandler&) = delete;

  void Rearm() { pending_.store(false); }

  // IUIAutomationStructureChangedEventHandler:
  IFACEMETHODIMP HandleStructureChangedEvent(IUIAutomationElement* sender,
                                             StructureChangeType change_type,
                                             SAFEARRAY* runtime_id) override {
    if (!pending_.exchange(true)) {
      on_changed_.Run();
    }
    return S_OK;
  }

 private:
  ~StructureChangedHandler() override = default;

  std::atomic<bool> pending_{false};
  const base::RepeatingClosure on_changed_;
};

// Returns the runner for the worker's thread. One thread, used by nothing else:
// - UIA event handlers must not be added or removed from more than one thread:
//   "A UI Automation client should not use multiple threads to add or remove
//   event handlers"; see
//   https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-eventsforclients.
// - The worker puts the thread in the MTA for the thread's whole life.
// - A UIA call hung in the target app must not hold up other tasks.
// CONTINUE_ON_SHUTDOWN, so that such a call cannot hold up process exit either.
scoped_refptr<base::SingleThreadTaskRunner> CreateWorkerTaskRunner() {
  return base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
}

// Returns why FindElement(`window`, `query`) is a request the browser never
// makes, or nullopt if it is not. These are bugs in the caller, unlike a window
// that has since closed, which any request can run into and which is answered
// with nullopt. Some never get this far: an invalid `window.process` (the field
// is non-nullable) and a `query` naming nothing to find (its StructTraits
// reject it) fail Mojo's own validation.
std::optional<std::string_view> GetBadRequestReason(
    const UiaWindowIdentity& window,
    const UiaElementQuery& query) {
  if (!window.hwnd) {
    return "FindElement() without a window";
  }

  if (window.window_class.empty()) {
    return "FindElement() without a window class";
  }

  if (IsChromeWindowClass(window.window_class)) {
    return "FindElement() for a Chrome window";
  }

  if (query.wait_for.is_negative()) {
    return "FindElement() with a negative wait";
  }

  return std::nullopt;
}

}  // namespace

// Owns all UIA state on a dedicated MTA thread.
class UiaElementProbeImpl::Worker {
 public:
  using ReplyCallback =
      base::OnceCallback<void(std::optional<UiaElementMeasurement>)>;

  Worker();

  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  ~Worker();

  void FindElement(UiaWindowIdentity window,
                   UiaElementQuery query,
                   ReplyCallback reply);

 private:
  struct Wait;

  // Returns the UIA client, creating it on first use, which loads
  // uiautomationcore.dll. Returns null if it cannot be created.
  IUIAutomation* GetOrCreateUia() VALID_CONTEXT_REQUIRED(sequence_checker_);

  void OnTreeChanged() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void ScheduleSearch(base::TimeDelta delay)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void SearchAgain() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void EndWait(std::optional<UiaElementMeasurement> measurement)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Keeps the dedicated thread alive until `~Worker()` finishes, since
  // `~SequenceBound()` drops its runner reference right after posting deletion
  // and dedicated threads skip remaining queued tasks once unreferenced.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();

  // UIA event handlers require an MTA thread. See
  // https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-threading#threading-model-for-event-handlers.
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};

  // CUIAutomation8, for IUIAutomation2's timeouts. Null until first use.
  ComPtr<IUIAutomation2> uia_;

  // Declared after `uia_` so `~Wait()` unregisters its handler while `uia_` is
  // still valid.
  std::unique_ptr<Wait> wait_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::OneShotTimer search_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer wait_deadline_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Worker> weak_ptr_factory_{this};
};

struct UiaElementProbeImpl::Worker::Wait {
  Wait() = default;

  ~Wait() {
    if (handler) {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      // Blocks until any active handler callback returns.
      uia->RemoveStructureChangedEventHandler(root.Get(), handler.Get());
    }
  }

  HWND hwnd = nullptr;
  base::Process process;
  std::wstring window_class;
  UiaElementQuery query;
  ReplyCallback reply;

  ComPtr<IUIAutomation> uia;
  ComPtr<IUIAutomationElement> root;
  ComPtr<StructureChangedHandler> handler;
};

UiaElementProbeImpl::Worker::Worker() {
  CHECK(com_initializer_.Succeeded());
  base::win::AssertComApartmentType(base::win::ComApartmentType::MTA);
}

UiaElementProbeImpl::Worker::~Worker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UiaElementProbeImpl::Worker::FindElement(UiaWindowIdentity window,
                                              UiaElementQuery query,
                                              ReplyCallback reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EndWait(std::nullopt);

  // Guaranteed by the query's StructTraits.
  DCHECK(!query.automation_id.empty() ||
         !query.fallback_automation_id_fragment.empty());

  const HWND hwnd = window.hwnd;
  base::Process process = std::move(window.process);
  const std::wstring& window_class = window.window_class;

  // Validate before loading uiautomationcore.dll.
  if (!IsExpectedWindow(hwnd, process, window_class)) {
    std::move(reply).Run(std::nullopt);
    return;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  IUIAutomation* uia = GetOrCreateUia();
  if (!uia) {
    std::move(reply).Run(std::nullopt);
    return;
  }

  // Checked again right before ElementFromHandle(), which sends the window its
  // first message (WM_GETOBJECT). The first UIA load reads
  // uiautomationcore.dll from disk and can take long enough for the window to
  // close, and in the worst case for its HWND to be reused by another window.
  if (!IsExpectedWindow(hwnd, process, window_class)) {
    std::move(reply).Run(std::nullopt);
    return;
  }

  ComPtr<IUIAutomationElement> root;
  if (FAILED(uia->ElementFromHandle(hwnd, &root)) || !root) {
    std::move(reply).Run(std::nullopt);
    return;
  }

  std::optional<UiaElementMeasurement> measurement =
      Search(uia, root.Get(), hwnd, query);
  const base::TimeDelta wait_for =
      std::clamp(query.wait_for, base::TimeDelta(), kMaxWait);
  if (measurement || wait_for.is_zero()) {
    std::move(reply).Run(std::move(measurement));
    return;
  }

  auto wait = std::make_unique<Wait>();
  wait->hwnd = hwnd;
  wait->process = std::move(process);
  wait->window_class = std::move(window.window_class);
  wait->query = std::move(query);
  wait->reply = std::move(reply);
  wait->uia = uia;
  wait->root = root;

  auto handler = Microsoft::WRL::Make<StructureChangedHandler>(
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &Worker::OnTreeChanged, weak_ptr_factory_.GetWeakPtr())));
  if (SUCCEEDED(uia->AddStructureChangedEventHandler(
          root.Get(), TreeScope_Subtree, /*cacheRequest=*/nullptr,
          handler.Get()))) {
    wait->handler = std::move(handler);
  }
  wait_ = std::move(wait);

  ScheduleSearch(kFallbackPollInterval);
  wait_deadline_.Start(
      FROM_HERE, wait_for,
      base::BindOnce(&Worker::EndWait, weak_ptr_factory_.GetWeakPtr(),
                     std::nullopt));
}

IUIAutomation* UiaElementProbeImpl::Worker::GetOrCreateUia() {
  if (!uia_) {
    if (FAILED(::CoCreateInstance(CLSID_CUIAutomation8, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia_)))) {
      uia_.Reset();
      return nullptr;
    }
    uia_->put_ConnectionTimeout(kUiaTimeoutMs);
    uia_->put_TransactionTimeout(kUiaTimeoutMs);
  }
  return uia_.Get();
}

void UiaElementProbeImpl::Worker::OnTreeChanged() {
  if (!wait_) {
    return;
  }
  // Never delay an already-scheduled earlier search.
  if (search_timer_.IsRunning() &&
      search_timer_.desired_run_time() <=
          base::TimeTicks::Now() + kChangeSettleDelay) {
    return;
  }
  ScheduleSearch(kChangeSettleDelay);
}

void UiaElementProbeImpl::Worker::ScheduleSearch(base::TimeDelta delay) {
  search_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&Worker::SearchAgain, weak_ptr_factory_.GetWeakPtr()));
}

void UiaElementProbeImpl::Worker::SearchAgain() {
  if (!wait_) {
    return;
  }

  if (!IsExpectedWindow(wait_->hwnd, wait_->process, wait_->window_class)) {
    EndWait(std::nullopt);
    return;
  }

  // Rearm before searching so changes during Search() schedule a follow-up.
  if (wait_->handler) {
    wait_->handler->Rearm();
  }

  std::optional<UiaElementMeasurement> measurement =
      Search(wait_->uia.Get(), wait_->root.Get(), wait_->hwnd, wait_->query);
  if (measurement) {
    EndWait(std::move(measurement));
    return;
  }
  ScheduleSearch(kFallbackPollInterval);
}

void UiaElementProbeImpl::Worker::EndWait(
    std::optional<UiaElementMeasurement> measurement) {
  search_timer_.Stop();
  wait_deadline_.Stop();
  if (!wait_) {
    return;
  }
  ReplyCallback reply = std::move(wait_->reply);
  wait_.reset();
  std::move(reply).Run(std::move(measurement));
}

UiaElementProbeImpl::UiaElementProbeImpl(
    mojo::PendingReceiver<chrome::mojom::UiaElementProbe> receiver)
    : worker_(CreateWorkerTaskRunner()), receiver_(this, std::move(receiver)) {}

UiaElementProbeImpl::~UiaElementProbeImpl() = default;

void UiaElementProbeImpl::FindElement(UiaWindowIdentity window,
                                      const UiaElementQuery& query,
                                      FindElementCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  if (const std::optional<std::string_view> reason =
          GetBadRequestReason(window, query)) {
    // Closes the pipe, after which the service shuts down.
    receiver_.ReportBadMessage(*reason);
    return;
  }

  // Reply immediately on this sequence in case the worker is blocked in UIA.
  if (pending_callback_) {
    // Drops the worker's eventual answer to the superseded call.
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(pending_callback_).Run(std::nullopt);
  }

  pending_callback_ = std::move(callback);
  worker_.AsyncCall(&Worker::FindElement)
      .WithArgs(
          std::move(window), query,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &UiaElementProbeImpl::OnFound, weak_ptr_factory_.GetWeakPtr())));
}

void UiaElementProbeImpl::OnFound(
    std::optional<UiaElementMeasurement> measurement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(pending_callback_).Run(measurement);
}
