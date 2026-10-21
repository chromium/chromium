// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/direct_launch/direct_launch_manager.h"

#include <objbase.h>

#include <windows.h>

#include <propsys.h>
#include <wrl/client.h>

#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/aligned_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/win/limited_access_features.h"
#include "base/win/propvarutil.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/win/direct_launch/direct_launch_features.h"
#include "chrome/browser/win/direct_launch/direct_launch_histograms.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/util_constants.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "url/url_constants.h"

namespace direct_launch {

namespace {

// The shell's direct launch API, which is not declared in the Windows SDK.
class __declspec(uuid("6fb3e48c-ba77-4f80-b9c9-77635a08a7f8"))
IExperimentalAPIInvoker : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE IsSupported(PCWSTR method_name,
                                                BOOL* is_supported) = 0;
  virtual HRESULT STDMETHODCALLTYPE Invoke(PCWSTR method_name,
                                           INamedPropertyStore* args,
                                           BOOL* is_supported) = 0;
};

// CLSID_ExperimentalAPIInvoker.
constexpr CLSID kExperimentalApiInvokerClsid = {
    0x81AF2611,
    0xE262,
    0x4090,
    {0xA1, 0x5B, 0xC0, 0x88, 0x95, 0xB1, 0xA0, 0xF0}};

// The methods of the direct launch API, and the names of their arguments.
constexpr wchar_t kRegisterUriLaunchMethod[] = L"RegisterAcceleratedUriLaunch";
constexpr wchar_t kUnregisterUriLaunchMethod[] =
    L"UnregisterAcceleratedUriLaunch";
constexpr wchar_t kRegisterTileLaunchMethod[] =
    L"RegisterAcceleratedTileLaunch";
constexpr wchar_t kUnregisterTileLaunchMethod[] =
    L"UnregisterAcceleratedTileLaunch";
constexpr wchar_t kTargetWindowArg[] = L"targetWindow";
constexpr wchar_t kSchemesArg[] = L"schemes";
constexpr wchar_t kCopyDataFormatIdArg[] = L"copyDataFormatId";
constexpr wchar_t kMessageIdArg[] = L"messageId";

// Upper bound on a direct launch payload. The buffer is sized by another
// process, so cap it at the longest URL Chrome will handle rather than
// allocating whatever length the sender happens to declare.
constexpr size_t kMaxDirectLaunchUriBytes = url::kMaxURLChars * sizeof(wchar_t);

// Overrides whether the OS supports direct launch when set.
constinit std::optional<bool> g_is_supported_for_testing;

// The copy data format id registered with the shell. A direct launch payload
// echoes this value back in `COPYDATASTRUCT::dwData`. `gfx::SingletonHwnd` is
// shared and any same-user process can post `WM_COPYDATA` to it, so a
// well-known id would barely narrow that traffic - and an id of 0 would not
// narrow it at all, being exactly what every sender that never sets `dwData`
// leaves behind. Draw a random nonzero id instead: unrelated senders do not
// carry it, and a process that did not observe the registration cannot guess
// it. It is still only a filter and not an authentication check, so the bounds
// and shape validation in `OnWndProc()` is what makes the payload safe to read.
uint32_t GenerateCopyDataFormatId() {
  // RandGenerator() returns a value in [0, range), so this yields [1, max].
  return static_cast<uint32_t>(
      base::RandGenerator(std::numeric_limits<uint32_t>::max()) + 1);
}

// Stable name for `result` in a trace, so a rejection is readable in an ETW
// capture without cross-referencing enums.xml. Deliberately a switch with no
// default: a new result fails to compile until it is named here.
const char* PayloadResultToTraceName(DirectLaunchPayloadResult result) {
  using Result = DirectLaunchPayloadResult;
  switch (result) {
    case Result::kAccepted:
      return "Accepted";
    case Result::kNullBuffer:
      return "NullBuffer";
    case Result::kTooSmall:
      return "TooSmall";
    case Result::kMisaligned:
      return "Misaligned";
    case Result::kOversized:
      return "Oversized";
    case Result::kEmpty:
      return "Empty";
  }
  // Only reachable if a value outside the enumeration is passed in. Placed
  // after the switch rather than in a `default:` case so that the compiler
  // still fails the build when a new result is left unnamed above.
  NOTREACHED();
}

// Unlocks the Limited Access Feature that gates the direct launch API.
bool UnlockDirectLaunchLimitedAccessFeature() {
  // Microsoft issued these tokens. Each one is bound to the Limited Access
  // Feature identity that chrome_exe.rc embeds for the branding.
  static constexpr wchar_t kToken[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      L"PgBt+ncNK6AhCwxT597KoQ==";
#else
      L"2nooLzdAga/lDfYjzlbyqw==";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::win::TryToUnlockLimitedAccessFeature(
      L"com.microsoft.windows.acceleratedlaunch.1", kToken);
}

// True when this process uses the default user data dir, and so is the
// instance that a shell launch would reach.
bool IsUsingDefaultUserDataDir() {
  // From the shell's point of view the registration is process-wide: the
  // ProgID is per install mode and user, with no profile dimension, so every
  // instance that registers displaces the previous one and any instance that
  // exits tears the registration down for the whole app.
  //
  // The ProgID command the shell registers is `chrome.exe --single-argument
  // %1`, which carries no --user-data-dir, so a classic launch always
  // rendezvouses with the instance that uses the default user data dir.
  // Restricting registration to that instance keeps direct launch a pure
  // optimization of the classic path: a URI can never be delivered to a
  // different user data dir than the one that would otherwise have handled it.
  // When that cannot be determined, launches are left to the classic path.
  return chrome::IsUsingDefaultDataDirectory().value_or(false);
}

// Creates the sequence that the calls into the shell run on. They may load
// libraries and block, so they run off the UI thread, but not at BEST_EFFORT
// priority: BEST_EFFORT tasks wait for startup to complete, which can take
// minutes, and launches take the classic path until the registration is in
// place. Withdrawing the registration at shutdown is best effort, so that a
// call into the shell can never hold up shutdown: should the registration
// outlive this process, as it does after a crash, the shell falls back to the
// classic launch, and the next instance's registration replaces it.
scoped_refptr<base::SequencedTaskRunner> CreateShellTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
}

// Returns whether `copydata` can be read as a direct launch URI, and if not,
// why. The payload is dictated by another process, so nothing about its size
// or shape can be assumed.
DirectLaunchPayloadResult ValidateDirectLaunchPayload(
    const COPYDATASTRUCT* copydata) {
  using Result = DirectLaunchPayloadResult;
  if (!copydata->lpData) {
    return Result::kNullBuffer;
  }
  if (copydata->cbData < sizeof(wchar_t)) {
    return Result::kTooSmall;
  }
  if (!base::IsAligned(copydata->lpData, alignof(wchar_t)) ||
      (copydata->cbData % sizeof(wchar_t)) != 0) {
    return Result::kMisaligned;
  }
  if (copydata->cbData > kMaxDirectLaunchUriBytes) {
    return Result::kOversized;
  }
  // A URI launch without a URI must not be handed on, since `Launch()` takes
  // an empty URI for a tile launch.
  if (*static_cast<const wchar_t*>(copydata->lpData) == L'\0') {
    return Result::kEmpty;
  }
  return Result::kAccepted;
}

// Sets the argument named `name` in `args` to `value`.
HRESULT SetUInt32Arg(INamedPropertyStore* args,
                     const wchar_t* name,
                     uint32_t value) {
  base::win::ScopedPropVariant variant;
  HRESULT hr = ::InitPropVariantFromUInt32(value, variant.Receive());
  if (FAILED(hr)) {
    return hr;
  }
  return args->SetNamedValue(name, variant.get());
}

// Creates the arguments that both registrations share, which direct the
// launches to `hwnd`.
HRESULT CreateRegistrationArgs(
    HWND hwnd,
    Microsoft::WRL::ComPtr<INamedPropertyStore>* args) {
  Microsoft::WRL::ComPtr<INamedPropertyStore> store;
  HRESULT hr = ::PSCreateMemoryPropertyStore(IID_PPV_ARGS(&store));
  if (FAILED(hr)) {
    return hr;
  }
  base::win::ScopedPropVariant target_window;
  hr = ::InitPropVariantFromInt64(reinterpret_cast<LONGLONG>(hwnd),
                                  target_window.Receive());
  if (FAILED(hr)) {
    return hr;
  }
  hr = store->SetNamedValue(kTargetWindowArg, target_window.get());
  if (FAILED(hr)) {
    return hr;
  }
  *args = std::move(store);
  return S_OK;
}

// Runs `method_name` of the direct launch API with `args`. The shell reports a
// method that it does not support through `is_supported` rather than through
// the HRESULT, so report that as E_NOTIMPL, so that a registration the shell
// ignored is not mistaken for one that it accepted.
HRESULT InvokeMethod(IExperimentalAPIInvoker* invoker,
                     PCWSTR method_name,
                     INamedPropertyStore* args) {
  BOOL is_supported = FALSE;
  const HRESULT hr = invoker->Invoke(method_name, args, &is_supported);
  if (SUCCEEDED(hr) && !is_supported) {
    return E_NOTIMPL;
  }
  return hr;
}

// Registers `hwnd` for URI launches of the http and https schemes, which the
// shell delivers as `WM_COPYDATA` tagged with `copy_data_format_id`.
HRESULT RegisterUriLaunch(IExperimentalAPIInvoker* invoker,
                          HWND hwnd,
                          uint32_t copy_data_format_id) {
  Microsoft::WRL::ComPtr<INamedPropertyStore> args;
  HRESULT hr = CreateRegistrationArgs(hwnd, &args);
  if (FAILED(hr)) {
    return hr;
  }
  PCWSTR schemes[] = {L"http", L"https"};
  base::win::ScopedPropVariant schemes_variant;
  hr = ::InitPropVariantFromStringVector(schemes, std::size(schemes),
                                         schemes_variant.Receive());
  if (FAILED(hr)) {
    return hr;
  }
  hr = args->SetNamedValue(kSchemesArg, schemes_variant.get());
  if (FAILED(hr)) {
    return hr;
  }
  // The shell tags every payload it delivers with the copy data format id, so
  // that `OnWndProc()` can tell direct launch traffic apart from everything
  // else arriving on the shared window.
  hr = SetUInt32Arg(args.Get(), kCopyDataFormatIdArg, copy_data_format_id);
  if (FAILED(hr)) {
    return hr;
  }
  return InvokeMethod(invoker, kRegisterUriLaunchMethod, args.Get());
}

// Registers `hwnd` for Start menu tile launches, which the shell delivers as
// `message_id`.
HRESULT RegisterTileLaunch(IExperimentalAPIInvoker* invoker,
                           HWND hwnd,
                           UINT message_id) {
  Microsoft::WRL::ComPtr<INamedPropertyStore> args;
  HRESULT hr = CreateRegistrationArgs(hwnd, &args);
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetUInt32Arg(args.Get(), kMessageIdArg, message_id);
  if (FAILED(hr)) {
    return hr;
  }
  return InvokeMethod(invoker, kRegisterTileLaunchMethod, args.Get());
}

}  // namespace

// Lives on the shell sequence. The shell's registration is app-wide, and the
// calls that withdraw it carry no identity, so this tracks what the shell
// accepted from it, and withdraws only that.
class DirectLaunchManager::ShellRegistrar {
 public:
  // Checks whether the OS supports direct launch, and reports the result to
  // `on_support_checked`.
  explicit ShellRegistrar(base::OnceCallback<void(bool)> on_support_checked) {
    std::move(on_support_checked).Run(ProbeIsSupported());
  }

  ShellRegistrar(const ShellRegistrar&) = delete;
  ShellRegistrar& operator=(const ShellRegistrar&) = delete;

  ~ShellRegistrar() { Unregister(); }

  // Registers `hwnd` for direct launch, replacing any previous registration,
  // and reports the result to UMA. The shell delivers URI launches as
  // `WM_COPYDATA` tagged with `copy_data_format_id`, and tile launches as
  // `tile_launch_message_id`, unless it is 0.
  void Register(HWND hwnd,
                uint32_t copy_data_format_id,
                UINT tile_launch_message_id) {
    Unregister();

    Microsoft::WRL::ComPtr<IExperimentalAPIInvoker> invoker;
    HRESULT result =
        ::CoCreateInstance(kExperimentalApiInvokerClsid, nullptr,
                           CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&invoker));
    if (SUCCEEDED(result)) {
      result = RegisterUriLaunch(invoker.Get(), hwnd, copy_data_format_id);
    }
    if (SUCCEEDED(result)) {
      invoker_ = std::move(invoker);
      // Without a message id, the shell would deliver tile launches as
      // WM_NULL, which `OnWndProc()` cannot tell apart from other traffic, so
      // they would be lost. Leave them to the classic launch instead.
      if (tile_launch_message_id != 0) {
        result =
            RegisterTileLaunch(invoker_.Get(), hwnd, tile_launch_message_id);
        if (SUCCEEDED(result)) {
          tile_registered_ = true;
        } else {
          Unregister();
        }
      }
    }
    base::UmaHistogramSparse(kDirectLaunchRegistrationResultHistogram, result);
  }

 private:
  // Checks whether the OS supports direct launch.
  static bool ProbeIsSupported() {
    if (g_is_supported_for_testing.has_value()) {
      return *g_is_supported_for_testing;
    }

    // Direct launch is only supported on Windows 11 and later.
    if (base::win::GetVersion() < base::win::Version::WIN11) {
      return false;
    }

    if (!UnlockDirectLaunchLimitedAccessFeature()) {
      return false;
    }
    Microsoft::WRL::ComPtr<IExperimentalAPIInvoker> invoker;
    if (FAILED(::CoCreateInstance(kExperimentalApiInvokerClsid, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&invoker)))) {
      return false;
    }
    BOOL is_supported = FALSE;
    return SUCCEEDED(
               invoker->IsSupported(kRegisterUriLaunchMethod, &is_supported)) &&
           is_supported;
  }

  // Withdraws what the shell accepted from this instance, if anything.
  void Unregister() {
    if (!invoker_) {
      return;
    }
    BOOL is_supported = FALSE;
    invoker_->Invoke(kUnregisterUriLaunchMethod, nullptr, &is_supported);
    if (tile_registered_) {
      invoker_->Invoke(kUnregisterTileLaunchMethod, nullptr, &is_supported);
    }
    invoker_.Reset();
    tile_registered_ = false;
  }

  // The shell's direct launch API, which must be held while registered. Only
  // set while the shell holds a URI registration from this instance, so that
  // a registration that it declined is never withdrawn.
  Microsoft::WRL::ComPtr<IExperimentalAPIInvoker> invoker_;

  // Whether the shell holds a tile registration from this instance.
  bool tile_registered_ = false;
};

DirectLaunchManager::DirectLaunchManager(
    ProcessSingleton::NotificationCallback launch_callback,
    DefaultBrowserChangeSubscriber subscriber)
    : DirectLaunchManager(std::move(launch_callback),
                          std::move(subscriber),
                          CreateShellTaskRunner()) {}

DirectLaunchManager::DirectLaunchManager(
    ProcessSingleton::NotificationCallback launch_callback,
    DefaultBrowserChangeSubscriber subscriber,
    scoped_refptr<base::SequencedTaskRunner> shell_task_runner)
    : launch_callback_(std::move(launch_callback)),
      subscriber_(std::move(subscriber)),
      copy_data_format_id_(GenerateCopyDataFormatId()) {
  // An instance that does not use the default user data dir is the process
  // singleton only for its own user data dir, so it must not claim the
  // registration, which is app-wide. Screening it out first also keeps it out
  // of the support check, and out of the field trial.
  const bool is_using_default_user_data_dir = IsUsingDefaultUserDataDir();
  base::UmaHistogramBoolean(kDirectLaunchUserDataDirOverrideHistogram,
                            !is_using_default_user_data_dir);
  if (!is_using_default_user_data_dir) {
    return;
  }

  shell_registrar_.emplace(
      std::move(shell_task_runner),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &DirectLaunchManager::OnSupportChecked, weak_factory_.GetWeakPtr())));
}

DirectLaunchManager::~DirectLaunchManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void DirectLaunchManager::SetIsSupportedForTesting(
    std::optional<bool> is_supported) {
  g_is_supported_for_testing = is_supported;
}

void DirectLaunchManager::OnSupportChecked(bool is_supported) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramBoolean(kDirectLaunchSupportedHistogram, is_supported);
  // Check the feature only once the OS is known to support direct launch:
  // checking it activates the field trial, and a client that cannot use direct
  // launch must not be counted in the experiment population.
  if (!is_supported || !base::FeatureList::IsEnabled(kDirectLaunchManager)) {
    shell_registrar_.Reset();
    return;
  }

  // The message that the shell delivers tile launches as. The name does not
  // vary by brand or install mode, so every browser built from Chromium gets
  // the same id. That is safe: the shell delivers a tile launch only to the
  // window that the app whose tile was launched registered, so another
  // browser's tile launch never arrives here. And any process that posts the
  // message to this window itself can only open a new window, which it could
  // just as well do by running the browser.
  tile_launch_message_id_ =
      ::RegisterWindowMessage(L"ChromeDirectLaunchTileLaunchMessage");

  // Listen for the launches that the shell delivers.
  hwnd_subscription_ =
      gfx::SingletonHwnd::GetInstance()->RegisterCallback(base::BindRepeating(
          &DirectLaunchManager::OnWndProc, base::Unretained(this)));

  // Listen for default browser changes.
  auto on_default_browser_changed = base::BindRepeating(
      &DirectLaunchManager::OnDefaultBrowserChanged, base::Unretained(this));
  default_browser_subscription_ =
      std::move(subscriber_).Run(std::move(on_default_browser_changed));

  // The registration is retried when the default browser changes, so keep
  // listening even if the shell declines it.
  Register();
}

void DirectLaunchManager::Register() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The shell sequence never touches the window: it only hands its handle to
  // the shell, which delivers launches to the window as messages, and those
  // are dispatched on this thread, which owns it. The handle stays valid for
  // as long as the shell may hold it, since `gfx::SingletonHwnd` is never
  // destroyed.
  shell_registrar_.AsyncCall(&ShellRegistrar::Register)
      .WithArgs(gfx::SingletonHwnd::GetInstance()->hwnd(), copy_data_format_id_,
                tile_launch_message_id_);
}

void DirectLaunchManager::OnDefaultBrowserChanged(
    shell_integration::DefaultWebClientState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The shell ignores the registration while this browser is not the default
  // browser, so only renew it when this browser is, or might be, the default.
  if (state == shell_integration::IS_DEFAULT ||
      state == shell_integration::UNKNOWN_DEFAULT) {
    Register();
  }
}

void DirectLaunchManager::OnWndProc(HWND hwnd,
                                    UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (message == WM_COPYDATA) {
    const auto* copydata = reinterpret_cast<const COPYDATASTRUCT*>(lparam);
    // `gfx::SingletonHwnd` is shared, so `WM_COPYDATA` also arrives from
    // senders that have nothing to do with a direct launch. Drop anything not
    // tagged with the format id registered with the shell, so unrelated
    // traffic is not counted as a launch. See `GenerateCopyDataFormatId()` for
    // why this screens rather than authenticates.
    if (!copydata || copydata->dwData != copy_data_format_id_) {
      return;
    }
    // The payload is dictated by another process and cannot be trusted to be
    // NUL-terminated, so bound it by the sender-declared `cbData` instead of
    // scanning for a terminator that may not be there.
    const DirectLaunchPayloadResult result =
        ValidateDirectLaunchPayload(copydata);
    base::UmaHistogramEnumeration(kDirectLaunchPayloadResultHistogram, result);
    if (result != DirectLaunchPayloadResult::kAccepted) {
      // Also emit a trace event so a rejection can be lined up against the
      // shell-side activity in the same ETW capture, which the histogram
      // alone cannot do.
      TRACE_EVENT_INSTANT(
          "startup", "DirectLaunchManager::OnWndProc PayloadRejected", "reason",
          PayloadResultToTraceName(result), "cbData", copydata->cbData);
      return;
    }
    // The payload is only valid while this message is being handled, so the
    // view must not outlive it; `Launch()` copies what it needs.
    std::wstring_view uri(static_cast<const wchar_t*>(copydata->lpData),
                          copydata->cbData / sizeof(wchar_t));
    // Drop the terminator, and any padding beyond it, when one is present.
    Launch(uri.substr(0, uri.find(L'\0')));
  } else if (tile_launch_message_id_ != 0 &&
             message == tile_launch_message_id_) {
    base::UmaHistogramBoolean(kDirectLaunchTileLaunchHistogram, true);
    Launch(std::wstring_view());
  }
}

void DirectLaunchManager::Launch(std::wstring_view uri) {
  // Build the command line the way the shell does when it invokes the
  // browser's registered ProgID open verb, by reusing the helper that emits
  // that verb at registration time (`"<exe>" --single-argument %1`), so that
  // the two cannot drift apart. `base::CommandLine::FromString()` re-parses the
  // string with `::CommandLineToArgvW()`, which splits on whitespace and strips
  // quotes; --single-argument suppresses that and takes the remainder of the
  // command line verbatim. Without it, a URI containing an unencoded space or
  // quote would be corrupted: for example, an OAuth authorization URL, whose
  // "scope" parameter is a space-delimited list, would lose every query
  // parameter after the first space. See crbug.com/937179.
  std::wstring command_line =
      base::CommandLine(base::FilePath(installer::kChromeExe))
          .GetCommandLineStringForShell();
  // `GetCommandLineStringForShell()` always ends in the shell's "%1"
  // placeholder, which the URI replaces.
  static constexpr std::wstring_view kShellPlaceholder = L"%1";
  CHECK(command_line.ends_with(kShellPlaceholder));
  command_line.resize(command_line.size() - kShellPlaceholder.size());
  command_line.append(uri);
  base::CommandLine launch_command_line =
      base::CommandLine::FromString(command_line);
  // A rendezvous tags the command line that it forwards with how the launching
  // process was started (see `AttemptToNotifyRunningChrome()`), and without a
  // tag, Launch.Mode2 attributes the launch to how this process was started,
  // e.g. to the taskbar shortcut that started it. A direct launch has no
  // launching process, so tag it as a launch through the browser's app id:
  // Launch.Mode2 then counts a URI launch as a protocol handler launch, like
  // the classic launch it replaces, and a tile launch as an app id launch.
  launch_command_line.AppendSwitch(switches::kSourceAppId);
  launch_callback_.Run(std::move(launch_command_line),
                       /*current_directory=*/base::FilePath());
}

}  // namespace direct_launch
