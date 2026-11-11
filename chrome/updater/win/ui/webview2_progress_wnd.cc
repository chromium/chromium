// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2_progress_wnd.h"

#include <windows.h>

#include <shlobj.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/current_module.h"
#include "base/win/resource_util.h"
#include "base/win/scoped_co_mem.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/path_util.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/resources/resources.grh"
#include "chrome/updater/win/ui/ui_util.h"
#include "chrome/updater/win/ui/webview2ui.h"
#include "third_party/webview2/include/WebView2.h"
#include "ui/gfx/geometry/rect.h"

namespace updater::ui {
namespace {
constexpr base::TimeDelta kInitializationTimeout = base::Seconds(10);
constexpr base::TimeDelta kUpdateTimeout = base::Seconds(10);
}  // namespace

WebView2ProgressWnd::WebView2ProgressWnd()
    : language_info_(GetLanguageInfo(base::UTF8ToWide(GetTagLanguage()))) {
  set_window_style(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                   WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
  set_initial_class_style(CS_HREDRAW | CS_VREDRAW);
  set_window_class_name(L"WebView2ProgressWnd");
}

WebView2ProgressWnd::~WebView2ProgressWnd() = default;

void WebView2ProgressWnd::SetEventSink(ProgressWndEvents* events) {
  events_ = events;
}

// static
base::expected<std::wstring, HRESULT> WebView2ProgressWnd::LoadProgressPage() {
  void* data = nullptr;
  size_t size = 0;
  if (!base::win::GetResourceFromModule(CURRENT_MODULE(),
                                        IDR_WEBVIEW2_PROGRESS_HTML, RT_HTML,
                                        &data, &size) ||
      !size) {
    LOG(ERROR) << "WebView2 embedded progress page is missing or empty";
    return base::unexpected(HRESULT_FROM_WIN32(ERROR_RESOURCE_DATA_NOT_FOUND));
  }
  std::wstring page;
  if (!base::UTF8ToWide(static_cast<const char*>(data), size, &page)) {
    LOG(ERROR) << "Invalid UTF-8 in WebView2 progress page";
    return base::unexpected(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
  }
  return page;
}

HRESULT WebView2ProgressWnd::Initialize() {
  // WebView2 requires the calling thread to be a single-threaded apartment
  // (STA), so a thread already initialized as MTA can't host it.
  if (!com_initializer_.Succeeded()) {
    LOG(ERROR) << "Thread apartment failed to initialize as STA.";
    return CO_E_NOTINITIALIZED;
  }

  // WebView2 can't be hosted in an elevated process: the runtime always runs
  // `msedgewebview2.exe` at medium integrity, so it can't use a profile in a
  // location that is safe for an elevated process to access, and a
  // user-writable profile would expose the elevated process to tampering.
  // This check must come before any WebView2 API call, since the loader
  // honors user-controlled overrides such as WEBVIEW2_BROWSER_EXECUTABLE_FOLDER
  // when locating the runtime. The caller falls back to the native UI.
  if (::IsUserAnAdmin()) {
    LOG(WARNING) << "WebView2 UI is not supported in an elevated process.";
    return E_ACCESSDENIED;
  }

  base::expected<std::wstring, HRESULT> page = LoadProgressPage();
  if (!page.has_value()) {
    return page.error();
  }
  html_content_ = std::move(page.value());

  // WebView2 requires a writable profile. This process is not elevated, so
  // the per-user install directory is both writable by `msedgewebview2.exe`
  // and at the same privilege level as this process, regardless of the
  // updater scope on the command line.
  const std::optional<base::FilePath> profile_root =
      GetInstallDirectory(UpdaterScope::kUser);
  if (!profile_root) {
    LOG(ERROR) << "Failed to resolve the WebView2 profile directory.";
    return E_FAIL;
  }
  user_data_dir_ = profile_root->Append(FILE_PATH_LITERAL("UserData"));

  // Fail early if the WebView2 runtime is not installed, instead of finding
  // out asynchronously after the window has been shown.
  base::win::ScopedCoMem<wchar_t> version;
  if (const HRESULT hr =
          ::GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
      FAILED(hr)) {
    LOG(ERROR) << "WebView2 runtime is not available: " << std::hex << hr;
    return hr;
  }
  if (!version.get() || !*version.get()) {
    LOG(ERROR) << "WebView2 runtime reported an empty version.";
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }
  VLOG(1) << "WebView2 runtime version: " << base::WideToUTF8(version.get());
  return S_OK;
}

const base::FilePath& WebView2ProgressWnd::user_data_dir_for_testing() const {
  return user_data_dir_;
}

void WebView2ProgressWnd::Show() {
  CHECK(!user_data_dir_.empty()) << "Initialize() must succeed before Show()";
  CHECK(creation_failed_callback_)
      << "The creation failed callback must be set before Show()";

  RECT rc = {0, 0, 500, 400};
  ::AdjustWindowRect(&rc, window_style(), FALSE);

  // Center the window on the screen.
  const int screen_w = ::GetSystemMetrics(SM_CXSCREEN);
  const int screen_h = ::GetSystemMetrics(SM_CYSCREEN);
  const int width = rc.right - rc.left;
  const int height = rc.bottom - rc.top;

  const gfx::Rect bounds((screen_w - width) / 2, (screen_h - height) / 2, width,
                         height);

  // `Init` is fatal if the window can't be created, so `hwnd()` is valid.
  Init(nullptr, bounds);

  ::SetWindowTextW(
      hwnd(), GetInstallerDisplayName(bundle_name_, language_info_.translation)
                  .c_str());

  // Show and paint the window.
  ::ShowWindow(hwnd(), SW_SHOW);
  ::UpdateWindow(hwnd());
}

base::DictValue WebView2ProgressWnd::GetUIUpdateForTesting() const {
  return GetUIUpdate();
}

base::DictValue WebView2ProgressWnd::GetUIUpdate() const {
  std::string status_text =
      state_.complete && !state_.completion_text.empty()
          ? state_.completion_text
          : base::WideToUTF8(GetLocalizedString(state_.message_id,
                                                language_info_.translation));
  // Normalize every displayed status, including success and error summaries,
  // without changing the text received by the native progress window.
  base::ReplaceSubstringsAfterOffset(&status_text, 0, "\r\n", "\n");
  base::DictValue update;
  update.Set("statusText", std::move(status_text));
  update.Set("title", base::WideToUTF8(GetInstallerDisplayName(
                          bundle_name_, language_info_.translation)));
  update.Set("language", language_info_.language);
  update.Set("textDirection", language_info_.is_rtl ? "rtl" : "ltr");
  update.Set("cancelText",
             base::WideToUTF8(GetLocalizedString(IDS_CANCEL_INSTALLATION_BASE,
                                                 language_info_.translation)));
  update.Set("cancelingText",
             base::WideToUTF8(GetLocalizedString(IDS_CANCELING_BASE,
                                                 language_info_.translation)));
  update.Set("closeText",
             base::WideToUTF8(GetLocalizedString(IDS_UPDATER_CLOSE_BASE,
                                                 language_info_.translation)));
  update.Set("progress", state_.progress_pos);
  update.Set("isMarquee", state_.is_marquee);
  update.Set("complete", state_.complete);
  update.Set("appName", base::UTF16ToUTF8(state_.app_name));
  return update;
}

void WebView2ProgressWnd::SetState(ProgressState state) {
  state_ = std::move(state);
  state_dirty_ = true;
  PushStateToPage();
}

bool WebView2ProgressWnd::CanUseWebView() const {
  return lifecycle_ == Lifecycle::kInitializing ||
         lifecycle_ == Lifecycle::kRunning;
}

void WebView2ProgressWnd::PushStateToPage() {
  if (!CanUseWebView() || !is_webview_ready_ || update_in_flight_) {
    return;
  }
  const std::optional<std::string> json = base::WriteJson(GetUIUpdate());
  if (!json) {
    LOG(ERROR) << "Could not serialize a WebView2 progress update";
    ScheduleFallback(E_FAIL);
    return;
  }
  // Progress notifications are coalesced while this request is outstanding.
  // Their arrival must not move the deadline of a stuck script execution.
  state_dirty_ = false;
  update_in_flight_ = true;
  if (!StartTimeout(kUpdateTimerId, kUpdateTimeout)) {
    ScheduleFallback(E_FAIL);
    return;
  }
  const HRESULT hr =
      ExecuteUIUpdate(base::UTF8ToWide(base::StrCat({"updateUI(", *json, ")"})),
                      base::BindOnce(&WebView2ProgressWnd::OnUIUpdated,
                                     msg_handler_weak_factory_.GetWeakPtr()));
  if (FAILED(hr)) {
    ScheduleFallback(hr);
  }
}

HRESULT WebView2ProgressWnd::ExecuteUIUpdate(
    const std::wstring& script,
    base::OnceCallback<void(const std::wstring&)> on_complete) {
  return browser_ ? browser_->ExecuteScript(script, std::move(on_complete))
                  : E_POINTER;
}

void WebView2ProgressWnd::OnUIUpdated(const std::wstring& result) {
  if (!CanUseWebView() || !update_in_flight_) {
    return;
  }
  ::KillTimer(hwnd(), kUpdateTimerId);
  update_in_flight_ = false;
  // WebView2 can return S_OK with JSON null when JavaScript throws. The page
  // explicitly acknowledges successful rendering with true.
  if (result != L"true") {
    LOG(ERROR) << "WebView2 progress update did not acknowledge success";
    ScheduleFallback(E_FAIL);
    return;
  }
  ::KillTimer(hwnd(), kInitTimerId);
  lifecycle_ = Lifecycle::kRunning;
  if (state_dirty_) {
    PushStateToPage();
  }
}

void WebView2ProgressWnd::OnCheckingForUpdate() {
  SetState({.message_id = IDS_WAITING_TO_CONNECT_BASE});
}

void WebView2ProgressWnd::OnDownloading(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  SetState({.message_id = time_remaining && time_remaining->InSeconds() == 0
                              ? IDS_DOWNLOADING_COMPLETED_BASE
                              : IDS_DOWNLOADING_BASE,
            .progress_pos = pos,
            .is_marquee = pos <= 0,
            .app_name = app_name});
}

void WebView2ProgressWnd::OnInstalling(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  SetState({.message_id = IDS_INSTALLING_BASE,
            .progress_pos = pos,
            .is_marquee = pos <= 0,
            .app_name = app_name});
}

void WebView2ProgressWnd::OnComplete(
    const ObserverCompletionInfo& observer_info) {
  const bool success =
      IsCompletionCodeSuccess(GetBundleCompletionCode(observer_info));
  // Reuse the native UI's selection of detailed, already localized errors.
  const std::string message =
      success
          ? base::UTF16ToUTF8(observer_info.completion_text)
          : base::WideToUTF8(GetBundleCompletionErrorMessages(observer_info));
  SetState({.message_id = success ? IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE
                                  : IDS_INSTALL_UPDATER_FAILED_BASE,
            .progress_pos = 100,
            .is_marquee = false,
            .complete = true,
            .completion_text = message});
}

void WebView2ProgressWnd::OnUpdateAvailable(const std::string&,
                                            const std::u16string&,
                                            const base::Version&) {}
void WebView2ProgressWnd::OnWaitingToDownload(const std::string&,
                                              const std::u16string& app_name) {
  // Unlike the native window's blank waiting label, keep a localized
  // connection status visible alongside indeterminate progress.
  SetState({.message_id = IDS_WAITING_TO_CONNECT_BASE, .app_name = app_name});
}
void WebView2ProgressWnd::OnWaitingRetryDownload(const std::string&,
                                                 const std::u16string&,
                                                 base::Time) {}
void WebView2ProgressWnd::OnWaitingToInstall(const std::string&,
                                             const std::u16string& app_name) {
  SetState({.message_id = IDS_WAITING_TO_INSTALL_BASE, .app_name = app_name});
}
void WebView2ProgressWnd::OnPause() {}

bool WebView2ProgressWnd::StartTimeout(UINT_PTR id, base::TimeDelta timeout) {
  if (!::SetTimer(hwnd(), id, static_cast<UINT>(timeout.InMilliseconds()),
                  nullptr)) {
    PLOG(ERROR) << "Could not start WebView2 timeout timer";
    return false;
  }
  return true;
}

void WebView2ProgressWnd::StopTimeouts() {
  ::KillTimer(hwnd(), kInitTimerId);
  ::KillTimer(hwnd(), kUpdateTimerId);
}

LRESULT WebView2ProgressWnd::OnCreate(UINT, WPARAM, LPARAM) {
  lifecycle_ = Lifecycle::kInitializing;
  // The updater pumps Win32 messages, not base tasks, while this UI is shown.
  if (!StartTimeout(kInitTimerId, kInitializationTimeout)) {
    ScheduleFallback(E_FAIL);
    return 0;
  }
  const HRESULT hr = CreateBrowser();
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not start WebView2 creation: " << std::hex << hr;
    ScheduleFallback(hr);
  }
  return 0;
}

HRESULT WebView2ProgressWnd::CreateBrowser() {
  RECT client_rect = {};
  ::GetClientRect(hwnd(), &client_rect);
  browser_ = std::make_unique<WebView2UI>();
  browser_->SetFailureHandler(
      base::BindRepeating(&WebView2ProgressWnd::ScheduleFallback,
                          msg_handler_weak_factory_.GetWeakPtr()));
  return browser_->Create(
      hwnd(), client_rect, user_data_dir_,
      base::BindOnce(&WebView2ProgressWnd::OnWebViewCreated,
                     msg_handler_weak_factory_.GetWeakPtr()));
}

LRESULT WebView2ProgressWnd::OnWebViewCreateFailed(UINT, WPARAM, LPARAM) {
  CompleteFallback();
  return 0;
}

void WebView2ProgressWnd::ScheduleFallback(HRESULT) {
  if (!CanUseWebView() || !::IsWindow(hwnd())) {
    return;
  }
  lifecycle_ = Lifecycle::kFallbackPending;
  is_webview_ready_ = false;
  update_in_flight_ = false;
  StopTimeouts();
  // Leave the WebView2 callback before handing off to native UI.
  if (!::PostMessage(hwnd(), WM_WEBVIEW2_CREATE_FAILED, 0, 0)) {
    PLOG(ERROR) << "Could not post WebView2 fallback message";
  }
}

void WebView2ProgressWnd::CompleteFallback() {
  if (lifecycle_ != Lifecycle::kFallbackPending) {
    return;
  }
  StopTimeouts();
  lifecycle_ = Lifecycle::kSuperseded;
  CHECK(creation_failed_callback_);
  VLOG(1) << "Handing WebView2 UI over to native UI";
  // The controller retains this object throughout the handoff. Run while
  // visible so that the native window can take the foreground.
  std::move(creation_failed_callback_).Run();
  ::ShowWindow(hwnd(), SW_HIDE);
  if (!::PostMessage(hwnd(), WM_CLOSE, 0, 0)) {
    PLOG(ERROR) << "Could not post close to superseded WebView2 window";
  }
}

LRESULT WebView2ProgressWnd::OnTimer(UINT, WPARAM wparam, LPARAM) {
  if (wparam != kInitTimerId && wparam != kUpdateTimerId) {
    SetMsgHandled(FALSE);
    return 0;
  }
  // Win32 timers repeat; cancel on the first notification for one-shot
  // behavior.
  ::KillTimer(hwnd(), wparam);
  if (!CanUseWebView()) {
    return 0;
  }
  if ((wparam == kInitTimerId && lifecycle_ == Lifecycle::kInitializing) ||
      (wparam == kUpdateTimerId && update_in_flight_)) {
    LOG(ERROR) << "WebView2 "
               << (wparam == kInitTimerId ? "initialization"
                                          : "progress update")
               << " timed out";
    ScheduleFallback(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
  }
  return 0;
}

void WebView2ProgressWnd::OnWebViewCreated(HRESULT result) {
  if (!CanUseWebView()) {
    return;
  }
  if (FAILED(result)) {
    LOG(ERROR) << "WebView2 creation failed: " << std::hex << result;
    ScheduleFallback(result);
    return;
  }
  browser_->SetWebMessageHandler(
      base::BindRepeating(&WebView2ProgressWnd::OnWebMessageReceived,
                          msg_handler_weak_factory_.GetWeakPtr()));

  browser_->NavigateToString(html_content_);
}

void WebView2ProgressWnd::OnWebMessageReceived(const std::wstring& message) {
  if (!CanUseWebView()) {
    return;
  }
  // Handle messages from javascript.
  if (message == L"close_window") {
    // A page cannot dismiss an active installation through the completion
    // action.
    if (!state_.complete) {
      return;
    }
    lifecycle_ = Lifecycle::kClosing;
    StopTimeouts();
    ::PostMessage(hwnd(), WM_CLOSE, 0, 0);
  } else if (message == L"cancel_installation") {
    lifecycle_ = Lifecycle::kClosing;
    StopTimeouts();
    VLOG(2) << "User clicked cancel in the HTML UI.";
    if (events_ && !state_.complete) {
      events_->DoCancel();
    }
    ::PostMessage(hwnd(), WM_CLOSE, 0, 0);
  } else if (message == L"ui_failed") {
    LOG(ERROR) << "WebView2 page reported a script error";
    ScheduleFallback(E_FAIL);
  } else if (message == L"ui_ready") {
    if (!is_webview_ready_) {
      VLOG(2) << "HTML UI DOM is loaded and ready.";
      is_webview_ready_ = true;
      PushStateToPage();
    }
  } else {
    LOG(FATAL) << "Unrecognized web message: " << message;
  }
}

LRESULT WebView2ProgressWnd::OnSize(UINT, WPARAM, LPARAM) {
  // Resize WebView2.
  if (browser_) {
    RECT client_rect = {};
    ::GetClientRect(hwnd(), &client_rect);
    browser_->Resize(client_rect);
  }
  SetMsgHandled(FALSE);  // Let other handlers process `WM_SIZE` if needed.
  return 0;
}

LRESULT WebView2ProgressWnd::OnDpiChanged(UINT, WPARAM, LPARAM lparam) {
  ApplySuggestedWindowRect(hwnd(), lparam);
  return 0;
}

LRESULT WebView2ProgressWnd::OnDestroy(UINT, WPARAM, LPARAM) {
  const bool superseded = lifecycle_ == Lifecycle::kSuperseded;
  lifecycle_ = Lifecycle::kClosing;
  is_webview_ready_ = false;
  update_in_flight_ = false;
  StopTimeouts();
  browser_.reset();
  // Closing a superseded window must not end the native UI's message loop.
  if (!superseded) {
    ::PostQuitMessage(0);
  }
  SetMsgHandled(FALSE);
  return 0;
}

}  // namespace updater::ui
