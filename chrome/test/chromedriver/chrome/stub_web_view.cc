// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"

StubWebView::StubWebView(const std::string& id) : id_(id) {}

StubWebView::~StubWebView() {}

bool StubWebView::IsServiceWorker() const {
  return false;
}

std::string StubWebView::GetId() {
  return id_;
}

bool StubWebView::WasCrashed() {
  return false;
}

Status StubWebView::HandleEventsUntil(const ConditionalFunc& conditional_func,
                                      const Timeout& timeout) {
  return Status{kOk};
}

Status StubWebView::HandleReceivedEvents() {
  return Status(kOk);
}

Status StubWebView::GetUrl(std::string* url) {
  return Status(kOk);
}

Status StubWebView::Load(const std::string& url, const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::Reload(const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::Freeze(const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::Resume(const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::StartBidiServer(std::string bidi_mapper_script) {
  return Status{kOk};
}

Status StubWebView::PostBidiCommand(base::Value::Dict command) {
  return Status{kOk};
}

Status StubWebView::SendBidiCommand(base::Value::Dict command,
                                    const Timeout& timeout,
                                    base::Value::Dict& response) {
  return Status{kOk};
}

Status StubWebView::SendCommand(const std::string& cmd,
                                const base::Value::Dict& params) {
  return Status(kOk);
}

Status StubWebView::SendCommandFromWebSocket(const std::string& cmd,
                                             const base::Value::Dict& params,
                                             const int client_cmd_id) {
  return Status(kOk);
}

Status StubWebView::SendCommandAndGetResult(
    const std::string& cmd,
    const base::Value::Dict& params,
    std::unique_ptr<base::Value>* value) {
  return Status(kOk);
}

Status StubWebView::TraverseHistory(int delta, const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::EvaluateScript(const std::string& frame,
                                   const std::string& function,
                                   const bool await_promise,
                                   std::unique_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubWebView::CallFunction(const std::string& frame,
                                 const std::string& function,
                                 const base::Value::List& args,
                                 std::unique_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubWebView::CallUserAsyncFunction(
    const std::string& frame,
    const std::string& function,
    const base::Value::List& args,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubWebView::CallUserSyncScript(const std::string& frame,
                                       const std::string& script,
                                       const base::Value::List& args,
                                       const base::TimeDelta& timeout,
                                       std::unique_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubWebView::GetFrameByFunction(const std::string& frame,
                                       const std::string& function,
                                       const base::Value::List& args,
                                       std::string* out_frame) {
  return Status(kOk);
}

Status StubWebView::DispatchMouseEvents(const std::vector<MouseEvent>& events,
                                        const std::string& frame,
                                        bool async_dispatch_events) {
  return Status(kOk);
}

Status StubWebView::DispatchTouchEvent(const TouchEvent& event,
                                       bool async_dispatch_events) {
  return Status(kOk);
}

Status StubWebView::DispatchTouchEvents(const std::vector<TouchEvent>& events,
                                        bool async_dispatch_events) {
  return Status(kOk);
}

Status StubWebView::DispatchTouchEventWithMultiPoints(
    const std::vector<TouchEvent>& events,
    bool async_dispatch_events) {
  return Status(kOk);
}

Status StubWebView::DispatchKeyEvents(const std::vector<KeyEvent>& events,
                                      bool async_dispatch_events) {
  return Status(kOk);
}


Status StubWebView::GetCookies(base::Value* cookies,
                               const std::string& current_page_url) {
  return Status(kOk);
}

Status StubWebView::DeleteCookie(const std::string& name,
                                 const std::string& url,
                                 const std::string& domain,
                                 const std::string& path) {
  return Status(kOk);
}

Status StubWebView::AddCookie(const std::string& name,
                              const std::string& url,
                              const std::string& value,
                              const std::string& domain,
                              const std::string& path,
                              const std::string& same_site,
                              bool secure,
                              bool http_only,
                              double expiry) {
  return Status(kOk);
}

Status StubWebView::WaitForPendingNavigations(const std::string& frame_id,
                                              const Timeout& timeout,
                                              bool stop_load_on_timeout) {
  return Status(kOk);
}

Status StubWebView::IsPendingNavigation(const Timeout* timeout,
                                        bool* is_pending) const {
  return Status(kOk);
}

MobileEmulationOverrideManager* StubWebView::GetMobileEmulationOverrideManager()
    const {
  return nullptr;
}

Status StubWebView::OverrideGeolocation(const Geoposition& geoposition) {
  return Status(kOk);
}

Status StubWebView::OverrideNetworkConditions(
    const NetworkConditions& network_conditions) {
  return Status(kOk);
}

Status StubWebView::OverrideDownloadDirectoryIfNeeded(
    const std::string& download_directory) {
  return Status(kOk);
}

Status StubWebView::CaptureScreenshot(std::string* screenshot,
                                      const base::Value::Dict& params) {
  return Status(kOk);
}

Status StubWebView::PrintToPDF(const base::Value::Dict& params,
                               std::string* pdf) {
  return Status(kOk);
}

Status StubWebView::SetFileInputFiles(const std::string& frame,
                                      const base::Value& element,
                                      const std::vector<base::FilePath>& files,
                                      const bool append) {
  return Status(kOk);
}

Status StubWebView::TakeHeapSnapshot(std::unique_ptr<base::Value>* snapshot) {
  return Status(kOk);
}

Status StubWebView::StartProfile() {
  return Status(kOk);
}

Status StubWebView::EndProfile(std::unique_ptr<base::Value>* profile_data) {
  return Status(kOk);
}

Status StubWebView::SynthesizeTapGesture(int x,
                                         int y,
                                         int tap_count,
                                         bool is_long_press) {
  return Status(kOk);
}

Status StubWebView::SynthesizeScrollGesture(int x,
                                            int y,
                                            int xoffset,
                                            int yoffset) {
  return Status(kOk);
}

bool StubWebView::IsNonBlocking() const {
  return false;
}

Status StubWebView::GetFedCmTracker(FedCmTracker** out_tracker) {
  return Status(kUnknownCommand);
}

FrameTracker* StubWebView::GetFrameTracker() const {
  return nullptr;
}

std::unique_ptr<base::Value> StubWebView::GetCastSinks() {
  return std::make_unique<base::Value>();
}

std::unique_ptr<base::Value> StubWebView::GetCastIssueMessage() {
  return std::make_unique<base::Value>();
}

void StubWebView::SetFrame(const std::string& new_frame_id) {}

Status StubWebView::GetBackendNodeIdByElement(const std::string& frame,
                                              const base::Value& element,
                                              int* node_id) {
  return Status(kOk);
}

bool StubWebView::IsDetached() const {
  return false;
}

Status StubWebView::CallFunctionWithTimeout(
    const std::string& frame,
    const std::string& function,
    const base::Value::List& args,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  return Status{kOk};
}

bool StubWebView::IsDialogOpen() const {
  return false;
}

Status StubWebView::GetDialogMessage(std::string& message) const {
  return Status(kOk);
}

Status StubWebView::GetTypeOfDialog(std::string& type) const {
  return Status(kOk);
}

Status StubWebView::HandleDialog(bool accept,
                                 const std::optional<std::string>& text) {
  return Status(kOk);
}

WebView* StubWebView::FindContainerForFrame(const std::string& frame_id) {
  return nullptr;
}
