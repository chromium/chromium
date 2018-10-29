// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"

#include <stddef.h>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/automation_extension.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/timeout.h"

#if defined(OS_POSIX)
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

// Enables wifi and data only, not airplane mode.
const int kDefaultConnectionType = 6;

bool KillProcess(const base::Process& process, bool kill_gracefully) {
#if defined(OS_POSIX)
  if (!kill_gracefully) {
    kill(process.Pid(), SIGKILL);
    base::TimeTicks deadline =
        base::TimeTicks::Now() + base::TimeDelta::FromSeconds(30);
    while (base::TimeTicks::Now() < deadline) {
      pid_t pid = HANDLE_EINTR(waitpid(process.Pid(), NULL, WNOHANG));
      if (pid == process.Pid())
        return true;
      if (pid == -1) {
        if (errno == ECHILD) {
          // The wait may fail with ECHILD if another process also waited for
          // the same pid, causing the process state to get cleaned up.
          return true;
        }
        LOG(WARNING) << "Error waiting for process " << process.Pid();
      }
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
    }
    return false;
  }
#endif

  if (!process.Terminate(0, true)) {
    int exit_code;
    return base::GetTerminationStatus(process.Handle(), &exit_code) !=
        base::TERMINATION_STATUS_STILL_RUNNING;
  }
  return true;
}

}  // namespace

ChromeDesktopImpl::ChromeDesktopImpl(
    std::unique_ptr<DevToolsHttpClient> http_client,
    std::unique_ptr<DevToolsClient> websocket_client,
    std::vector<std::unique_ptr<DevToolsEventListener>>
        devtools_event_listeners,
    std::string page_load_strategy,
    base::Process process,
    const base::CommandLine& command,
    base::ScopedTempDir* user_data_dir,
    base::ScopedTempDir* extension_dir,
    bool network_emulation_enabled)
    : ChromeImpl(std::move(http_client),
                 std::move(websocket_client),
                 std::move(devtools_event_listeners),
                 page_load_strategy),
      process_(std::move(process)),
      command_(command),
      network_connection_enabled_(network_emulation_enabled),
      network_connection_(kDefaultConnectionType) {
  if (user_data_dir->IsValid())
    CHECK(user_data_dir_.Set(user_data_dir->Take()));
  if (extension_dir->IsValid())
    CHECK(extension_dir_.Set(extension_dir->Take()));
}

ChromeDesktopImpl::~ChromeDesktopImpl() {
  if (!quit_) {
    base::FilePath user_data_dir = user_data_dir_.Take();
    base::FilePath extension_dir = extension_dir_.Take();
    LOG(WARNING) << "chrome quit unexpectedly, leaving behind temporary "
        "directories for debugging:";
    if (user_data_dir_.IsValid())
      LOG(WARNING) << "chrome user data directory: " << user_data_dir.value();
    if (extension_dir_.IsValid())
      LOG(WARNING) << "chromedriver automation extension directory: "
                   << extension_dir.value();
  }
}

Status ChromeDesktopImpl::WaitForPageToLoad(
    const std::string& url,
    const base::TimeDelta& timeout_raw,
    std::unique_ptr<WebView>* web_view,
    bool w3c_compliant) {
  Timeout timeout(timeout_raw);
  std::string id;
  WebViewInfo::Type type = WebViewInfo::Type::kPage;
  while (timeout.GetRemainingTime() > base::TimeDelta()) {
    WebViewsInfo views_info;
    Status status = devtools_http_client_->GetWebViewsInfo(&views_info);
    if (status.IsError())
      return status;

    for (size_t i = 0; i < views_info.GetSize(); ++i) {
      const WebViewInfo& view_info = views_info.Get(i);
      if (base::StartsWith(view_info.url, url, base::CompareCase::SENSITIVE)) {
        id = view_info.id;
        type = view_info.type;
        break;
      }
    }
    if (!id.empty())
      break;
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }
  if (id.empty())
    return Status(kUnknownError, "page could not be found: " + url);

  const DeviceMetrics* device_metrics = devtools_http_client_->device_metrics();
  if (type == WebViewInfo::Type::kApp ||
      type == WebViewInfo::Type::kBackgroundPage) {
    // Apps and extensions don't work on Android, so it doesn't make sense to
    // provide override device metrics in mobile emulation mode, and can also
    // potentially crash the renderer, for more details see:
    // https://code.google.com/p/chromedriver/issues/detail?id=1205
    device_metrics = nullptr;
  }
  std::unique_ptr<WebView> web_view_tmp(
      new WebViewImpl(id, w3c_compliant, devtools_http_client_->browser_info(),
                      devtools_http_client_->CreateClient(id), device_metrics,
                      page_load_strategy()));
  Status status = web_view_tmp->ConnectIfNecessary();
  if (status.IsError())
    return status;

  status = web_view_tmp->WaitForPendingNavigations(
      std::string(), timeout, false);
  if (status.IsOk())
    *web_view = std::move(web_view_tmp);
  return status;
}

Status ChromeDesktopImpl::GetAutomationExtension(
    AutomationExtension** extension,
    bool w3c_compliant) {
  if (!automation_extension_) {
    std::unique_ptr<WebView> web_view;
    Status status = WaitForPageToLoad(
        "chrome-extension://aapnijgdinlhnhlmodcfapnahmbfebeb/"
        "_generated_background_page.html",
        base::TimeDelta::FromSeconds(10),
        &web_view,
        w3c_compliant);
    if (status.IsError())
      return Status(kUnknownError, "cannot get automation extension", status);

    // The automation extension page has been loaded, but it might not be
    // initialized yet. Wait for up to 10 seconds for a function on the page
    // to become defined, as a signal that the page is initialized.
    base::TimeTicks deadline =
        base::TimeTicks::Now() + base::TimeDelta::FromSeconds(10);
    while (base::TimeTicks::Now() < deadline) {
      std::unique_ptr<base::Value> result;
      status = web_view->EvaluateScript(
          std::string(), "typeof launchApp === 'function'", &result);
      if (status.IsError())
        return Status(kUnknownError, "cannot get automation extension", status);
      if (result->is_bool() && result->GetBool())
        break;
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
    }

    automation_extension_.reset(new AutomationExtension(std::move(web_view)));
  }
  *extension = automation_extension_.get();
  return Status(kOk);
}

Status ChromeDesktopImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  *desktop = this;
  return Status(kOk);
}

std::string ChromeDesktopImpl::GetOperatingSystemName() {
  return base::SysInfo::OperatingSystemName();
}

bool ChromeDesktopImpl::IsMobileEmulationEnabled() const {
  return devtools_http_client_->device_metrics() != NULL;
}

bool ChromeDesktopImpl::HasTouchScreen() const {
  return IsMobileEmulationEnabled();
}

bool ChromeDesktopImpl::IsNetworkConnectionEnabled() const {
  return network_connection_enabled_;
}

Status ChromeDesktopImpl::QuitImpl() {
  // If the Chrome session uses a custom user data directory, try sending a
  // SIGTERM signal before SIGKILL, so that Chrome has a chance to write
  // everything back out to the user data directory and exit cleanly. If we're
  // using a temporary user data directory, we're going to delete the temporary
  // directory anyway, so just send SIGKILL immediately.
  bool kill_gracefully = !user_data_dir_.IsValid();
  // If the Chrome session is being run with --log-net-log, send SIGTERM first
  // to allow Chrome to write out all the net logs to the log path.
  kill_gracefully |= command_.HasSwitch("log-net-log");
  if (!KillProcess(process_, kill_gracefully))
    return Status(kUnknownError, "cannot kill Chrome");
  return Status(kOk);
}

const base::CommandLine& ChromeDesktopImpl::command() const {
  return command_;
}

int ChromeDesktopImpl::GetNetworkConnection() const {
  return network_connection_;
}

void ChromeDesktopImpl::SetNetworkConnection(
    int network_connection) {
  network_connection_ = network_connection;
}
