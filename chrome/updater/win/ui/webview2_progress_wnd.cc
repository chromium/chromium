// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2_progress_wnd.h"

#include <windows.h>

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
#include "chrome/updater/get_updater_scope.h"
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

  base::expected<std::wstring, HRESULT> page = LoadProgressPage();
  if (!page.has_value()) {
    return page.error();
  }
  html_content_ = std::move(page.value());

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

  std::optional<base::FilePath> install_dir =
      GetInstallDirectory(GetUpdaterScope());
  if (!install_dir) {
    LOG(ERROR) << "Failed to resolve installation directory.";
    return E_FAIL;
  }
  user_data_dir_ = install_dir->Append(FILE_PATH_LITERAL("UserData"));
  return S_OK;
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
  update.Set("progress", state_.progress_pos);
  update.Set("isMarquee", state_.is_marquee);
  update.Set("complete", state_.complete);
  update.Set("appName", base::UTF16ToUTF8(state_.app_name));
  return update;
}

void WebView2ProgressWnd::SetState(ProgressState state) {
  state_ = std::move(state);
  PushStateToPage();
}

void WebView2ProgressWnd::PushStateToPage() {
  if (!is_webview_ready_ || !browser_ || superseded_) {
    return;
  }

  const std::optional<std::string> json = base::WriteJson(GetUIUpdate());
  if (!json) {
    LOG(ERROR) << "Could not serialize a WebView2 progress update";
    return;
  }
  browser_->ExecuteScript(
      base::UTF8ToWide(base::StrCat({"updateUI(", *json, ")"})));
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

LRESULT WebView2ProgressWnd::OnCreate(UINT, WPARAM, LPARAM) {
  RECT client_rect = {0};
  ::GetClientRect(hwnd(), &client_rect);

  browser_ = std::make_unique<WebView2UI>();
  const HRESULT hr =
      browser_->Create(hwnd(), client_rect, user_data_dir_,
                       base::BindOnce(&WebView2ProgressWnd::OnWebViewCreated,
                                      msg_handler_weak_factory_.GetWeakPtr()));
  if (FAILED(hr)) {
    // The window is still being created, so the failure can't be handled
    // here. Returning -1 is not an option either: `gfx::WindowImpl::Init`
    // treats a failed window creation as fatal. Instead, handle the failure
    // once the UI message loop runs, through the same path as an asynchronous
    // WebView2 creation failure.
    ::PostMessage(hwnd(), WM_WEBVIEW2_CREATE_FAILED, static_cast<WPARAM>(hr),
                  0);
  }
  return 0;
}

LRESULT WebView2ProgressWnd::OnWebViewCreateFailed(UINT,
                                                   WPARAM wparam,
                                                   LPARAM) {
  OnWebViewCreationFailed(static_cast<HRESULT>(wparam));
  return 0;
}

void WebView2ProgressWnd::OnWebViewCreationFailed(HRESULT result) {
  LOG(ERROR) << "Failed to create WebView2: " << std::hex << result;
  CHECK(creation_failed_callback_);

  // Hand the UI over to the fallback. Once superseded, destroying this window
  // must not quit the UI message loop, which the fallback UI keeps using.
  superseded_ = true;

  // Run the callback while this window is still visible: the fallback window
  // can only take the foreground while this process owns the foreground
  // window.
  std::move(creation_failed_callback_).Run();

  // This may be running from within a WebView2 callback, so the window is
  // hidden now and closed asynchronously.
  ::ShowWindow(hwnd(), SW_HIDE);
  ::PostMessage(hwnd(), WM_CLOSE, 0, 0);
}

void WebView2ProgressWnd::OnWebViewCreated(HRESULT result) {
  if (superseded_) {
    return;
  }
  if (FAILED(result)) {
    OnWebViewCreationFailed(result);
    return;
  }
  browser_->SetWebMessageHandler(
      base::BindRepeating(&WebView2ProgressWnd::OnWebMessageReceived,
                          msg_handler_weak_factory_.GetWeakPtr()));

  if (const HRESULT hr = browser_->NavigateToString(html_content_);
      FAILED(hr)) {
    LOG(ERROR) << "Failed to navigate to the progress page: " << std::hex << hr;
    OnWebViewCreationFailed(hr);
    return;
  }
}

void WebView2ProgressWnd::OnWebMessageReceived(const std::wstring& message) {
  if (superseded_) {
    return;
  }
  // Handle messages from javascript.
  if (message == L"cancel_installation") {
    VLOG(2) << "User clicked cancel in the HTML UI.";
    if (events_) {
      events_->DoCancel();
    }
    ::PostMessage(hwnd(), WM_CLOSE, 0, 0);
  } else if (message == L"ui_ready") {
    VLOG(2) << "HTML UI DOM is loaded and ready.";
    is_webview_ready_ = true;
    PushStateToPage();
  } else {
    LOG(FATAL) << "Unrecognized web message: " << message;
  }
}

LRESULT WebView2ProgressWnd::OnSize(UINT, WPARAM, LPARAM) {
  // Resize WebView2.
  if (browser_) {
    RECT client_rect = {0};
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
  // Explicitly destroy the browser to release COM references and close the
  // underlying WebView2 processes.
  browser_.reset();

  // Closing the window ends the UI, unless another window has taken over.
  if (!superseded_) {
    ::PostQuitMessage(0);
  }

  SetMsgHandled(FALSE);
  return 0;
}

}  // namespace updater::ui
