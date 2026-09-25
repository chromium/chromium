// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/test/base/drag_and_drop_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

namespace drag_and_drop_test_utils {

namespace {
// These are ui::DropTargetEvent::source_operations_ being sent when manually
// trying out drag&drop of an image file from Nemo (Ubuntu's file explorer)
// into a content_shell.
constexpr int kDefaultSourceOperations = ui::DragDropTypes::DRAG_MOVE |
                                         ui::DragDropTypes::DRAG_COPY |
                                         ui::DragDropTypes::DRAG_LINK;
}  // namespace

struct DragAndDropSimulator::PlatformState {
  PlatformState(content::WebContents* drag_contents,
                content::WebContents* drop_contents)
      : drag_contents_(drag_contents), drop_contents_(drop_contents) {}

  aura::client::DragDropDelegate* GetDragDelegate();
  aura::client::DragDropDelegate* GetDropDelegate();
  aura::client::DragDropDelegate* GetOmniboxDragDropDelegate(
      aura::Window* omnibox);
  void CalculateEventLocations(const gfx::Point& web_contents_relative_location,
                               gfx::PointF* out_event_location,
                               gfx::PointF* out_event_root_location,
                               content::WebContents* contents);

  raw_ptr<content::WebContents> drag_contents_;
  raw_ptr<content::WebContents> drop_contents_;
  std::unique_ptr<ui::DropTargetEvent> active_drag_event_;
  std::unique_ptr<ui::OSExchangeData> os_exchange_data_;
};

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* web_contents)
    : DragAndDropSimulator(web_contents, web_contents) {}

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* drag_contents,
                                           content::WebContents* drop_contents)
    : state_(std::make_unique<PlatformState>(drag_contents, drop_contents)) {}

DragAndDropSimulator::~DragAndDropSimulator() = default;

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const std::string& text) {
  state_->os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  state_->os_exchange_data_->SetString(base::UTF8ToUTF16(text));
  return SimulateDragEnter(location, std::move(state_->os_exchange_data_));
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const GURL& url) {
  state_->os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  state_->os_exchange_data_->SetURL(url, base::UTF8ToUTF16(url.spec()));
  return SimulateDragEnter(location, std::move(state_->os_exchange_data_));
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const base::FilePath& file) {
  state_->os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  state_->os_exchange_data_->SetFilename(file);
  return SimulateDragEnter(location, std::move(state_->os_exchange_data_));
}

bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    const std::vector<ui::FileInfo>& file_infos) {
  state_->os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  state_->os_exchange_data_->SetFilenames(file_infos);
  return SimulateDragEnter(location, std::move(state_->os_exchange_data_));
}

#if BUILDFLAG(IS_WIN)
bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    const std::vector<std::pair<base::FilePath, base::span<const uint8_t>>>&
        filenames_and_contents,
    DWORD tymed) {
  state_->os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  state_->os_exchange_data_->provider().SetVirtualFileContentsForTesting(
      filenames_and_contents, tymed);
  return SimulateDragEnter(location, std::move(state_->os_exchange_data_));
}
#endif  // BUILDFLAG(IS_WIN)

bool DragAndDropSimulator::SimulateOmniboxDragEnter(aura::Window* omnibox,
                                                    const gfx::Point& location,
                                                    const GURL& url) {
  state_->os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  state_->os_exchange_data_->SetURL(url, base::UTF8ToUTF16(url.spec()));
  if (state_->active_drag_event_) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }

  aura::client::DragDropDelegate* delegate =
      state_->GetOmniboxDragDropDelegate(omnibox);
  if (!delegate) {
    return false;
  }

  state_->active_drag_event_ = std::make_unique<ui::DropTargetEvent>(
      *state_->os_exchange_data_, gfx::PointF(location), gfx::PointF(location),
      kDefaultSourceOperations);

  delegate->OnDragEntered(*state_->active_drag_event_);
  delegate->OnDragUpdated(*state_->active_drag_event_);
  return true;
}

bool DragAndDropSimulator::SimulateDrop(const gfx::Point& location) {
  if (!state_->active_drag_event_) {
    ADD_FAILURE() << "Cannot drop a drag that hasn't started yet.";
    return false;
  }

  aura::client::DragDropDelegate* delegate = state_->GetDropDelegate();
  if (!delegate) {
    return false;
  }

  gfx::PointF event_location;
  gfx::PointF event_root_location;
  state_->CalculateEventLocations(location, &event_location,
                                  &event_root_location, state_->drop_contents_);
  state_->active_drag_event_->set_location_f(event_location);
  state_->active_drag_event_->set_root_location_f(event_root_location);

  delegate->OnDragUpdated(*state_->active_drag_event_);
  auto drop_cb = delegate->GetDropCallback(*state_->active_drag_event_);
  // 'drop_cb' should have a value because WebContentsViewAura
  // (DragDropDelegate) doesn't return NullCallback.
  DCHECK(drop_cb);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(std::move(state_->os_exchange_data_), output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  return true;
}

bool DragAndDropSimulator::SimulateOmniboxDrop(aura::Window* omnibox,
                                               const gfx::Point& location) {
  if (!state_->active_drag_event_) {
    ADD_FAILURE() << "Cannot drop a drag that hasn't started yet.";
    return false;
  }

  aura::client::DragDropDelegate* delegate =
      state_->GetOmniboxDragDropDelegate(omnibox);
  if (!delegate) {
    return false;
  }

  state_->active_drag_event_->set_location_f(gfx::PointF(location));
  state_->active_drag_event_->set_root_location_f(gfx::PointF(location));

  delegate->OnDragUpdated(*state_->active_drag_event_);
  auto drop_cb = delegate->GetDropCallback(*state_->active_drag_event_);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(std::move(state_->os_exchange_data_), output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  return true;
}

bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    std::unique_ptr<ui::OSExchangeData> data) {
  if (state_->active_drag_event_) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }

  aura::client::DragDropDelegate* delegate = state_->GetDragDelegate();
  if (!delegate) {
    return false;
  }

  CHECK(data);

  state_->os_exchange_data_ = std::move(data);

  gfx::PointF event_location;
  gfx::PointF event_root_location;
  state_->CalculateEventLocations(location, &event_location,
                                  &event_root_location, state_->drag_contents_);
  state_->active_drag_event_ = std::make_unique<ui::DropTargetEvent>(
      *state_->os_exchange_data_, event_location, event_root_location,
      kDefaultSourceOperations);

  delegate->OnDragEntered(*state_->active_drag_event_);
  delegate->OnDragUpdated(*state_->active_drag_event_);
  return true;
}

aura::client::DragDropDelegate*
DragAndDropSimulator::PlatformState::GetDragDelegate() {
  gfx::NativeView view = drag_contents_->GetContentNativeView();
  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(view);
  EXPECT_TRUE(delegate) << "Expecting WebContents to have DragDropDelegate";
  return delegate;
}

aura::client::DragDropDelegate*
DragAndDropSimulator::PlatformState::GetDropDelegate() {
  gfx::NativeView view = drop_contents_->GetContentNativeView();
  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(view);
  EXPECT_TRUE(delegate) << "Expecting WebContents to have DragDropDelegate";
  return delegate;
}

aura::client::DragDropDelegate*
DragAndDropSimulator::PlatformState::GetOmniboxDragDropDelegate(
    aura::Window* omnibox) {
  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(omnibox);
  EXPECT_TRUE(delegate) << "Expecting Omnibox to have DragDropDelegate";
  return delegate;
}

void DragAndDropSimulator::PlatformState::CalculateEventLocations(
    const gfx::Point& web_contents_relative_location,
    gfx::PointF* out_event_location,
    gfx::PointF* out_event_root_location,
    content::WebContents* contents) {
  gfx::NativeView view = contents->GetNativeView();

  *out_event_location = gfx::PointF(web_contents_relative_location);

  gfx::Point root_location = web_contents_relative_location;
  aura::Window::ConvertPointToTarget(view, view->GetRootWindow(),
                                     &root_location);
  *out_event_root_location = gfx::PointF(root_location);
}

struct DragStartWaiter::PlatformState : public aura::client::DragDropClient,
                                        public aura::WindowObserver {
  PlatformState(content::WebContents* web_contents,
                base::OnceClosure on_drag_started_callback);
  ~PlatformState() override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // aura::client::DragDropClient:
  ui::mojom::DragOperation StartDragAndDrop(
      std::unique_ptr<ui::OSExchangeData> data,
      aura::Window* root_window,
      aura::Window* source_window,
      const gfx::Point& screen_location,
      int allowed_operations,
      ui::mojom::DragEventSource source) override;
  void DragCancel() override;
#if BUILDFLAG(IS_LINUX)
  void UpdateDragImage(const gfx::ImageSkia& image,
                       const gfx::Vector2d& offset) override;
#endif
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override;
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override;

  raw_ptr<aura::Window> root_window_ = nullptr;
  raw_ptr<aura::client::DragDropClient> old_client_ = nullptr;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
  base::RunLoop run_loop_;
  base::RunLoop release_loop_;
  base::OnceClosure on_drag_started_callback_;
  std::unique_ptr<ui::OSExchangeData> captured_data_;
  bool suppress_passing_further_ = false;
};

DragStartWaiter::DragStartWaiter(content::WebContents* web_contents)
    : DragStartWaiter(web_contents, base::DoNothing()) {}

DragStartWaiter::DragStartWaiter(content::WebContents* web_contents,
                                 base::OnceClosure on_drag_started_callback)
    : state_(std::make_unique<PlatformState>(
          web_contents,
          std::move(on_drag_started_callback))) {}

DragStartWaiter::~DragStartWaiter() = default;

DragStartWaiter::PlatformState::PlatformState(
    content::WebContents* web_contents,
    base::OnceClosure on_drag_started_callback)
    : on_drag_started_callback_(std::move(on_drag_started_callback)) {
  CHECK(web_contents);
  CHECK(web_contents->GetContentNativeView());
  root_window_ = web_contents->GetContentNativeView()->GetRootWindow();
  CHECK(root_window_);
  old_client_ = aura::client::GetDragDropClient(root_window_);
  CHECK(old_client_);
  window_observation_.Observe(root_window_);
  aura::client::SetDragDropClient(root_window_, this);
}

DragStartWaiter::PlatformState::~PlatformState() {
  DragCancel();
  if (root_window_) {
    window_observation_.Reset();
    aura::client::SetDragDropClient(root_window_, old_client_);
    root_window_ = nullptr;
    old_client_ = nullptr;
  }
}

void DragStartWaiter::PlatformState::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (window == root_window_ &&
      aura::client::GetDragDropClient(root_window_) != this) {
    // If the host detaches the DragDropClient (e.g. in
    // DesktopNativeWidgetAura::OnHostWillClose() right before destroying
    // `old_client_`), drop `old_client_` immediately so it never dangles or
    // gets restored onto `root_window_`.
    window_observation_.Reset();
    root_window_ = nullptr;
    old_client_ = nullptr;
  }
}

void DragStartWaiter::PlatformState::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == window) {
    window_observation_.Reset();
    old_client_ = nullptr;
    aura::client::SetDragDropClient(root_window_, nullptr);
    root_window_ = nullptr;
  }
}

void DragStartWaiter::WaitUntilDragStart() {
  state_->run_loop_.Run();
}

void DragStartWaiter::ReleaseDrag() {
  state_->release_loop_.Quit();
}

std::unique_ptr<ui::OSExchangeData> DragStartWaiter::TakeCapturedData() {
  CHECK(state_->suppress_passing_further_ || state_->captured_data_)
      << "Cannot extract captured data unless drag is suppressed or completed.";
  return std::move(state_->captured_data_);
}

void DragStartWaiter::SuppressPassingStartDragFurther() {
  state_->suppress_passing_further_ = true;
}

ui::mojom::DragOperation DragStartWaiter::PlatformState::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int allowed_operations,
    ui::mojom::DragEventSource source) {
  CHECK(!on_drag_started_callback_ || suppress_passing_further_)
      << "on_drag_started_callback requires SuppressPassingStartDragFurther "
         "to be called.";
  captured_data_ = std::move(data);
  run_loop_.Quit();

  if (suppress_passing_further_) {
    if (on_drag_started_callback_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_drag_started_callback_));
    }
    release_loop_.Run();
    return ui::mojom::DragOperation::kCopy;
  }

  CHECK(captured_data_);
  return old_client_->StartDragAndDrop(std::move(captured_data_), root_window,
                                       source_window, screen_location,
                                       allowed_operations, source);
}

void DragStartWaiter::PlatformState::DragCancel() {
  release_loop_.Quit();
}

#if BUILDFLAG(IS_LINUX)
void DragStartWaiter::PlatformState::UpdateDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& offset) {}
#endif

bool DragStartWaiter::PlatformState::IsDragDropInProgress() {
  return captured_data_ != nullptr;
}

void DragStartWaiter::PlatformState::AddObserver(
    aura::client::DragDropClientObserver* observer) {}

void DragStartWaiter::PlatformState::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {}

}  // namespace drag_and_drop_test_utils
