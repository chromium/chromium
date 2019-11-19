// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_STUB_WEB_VIEW_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_STUB_WEB_VIEW_H_

#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

class StubWebView : public WebView {
 public:
  explicit StubWebView(const std::string& id);
  ~StubWebView() override;

  // Overridden from WebView:
  std::string GetId() override;
  bool WasCrashed() override;
  Status ConnectIfNecessary() override;
  Status HandleReceivedEvents() override;
  Status GetUrl(std::string* url) override;
  Status Load(const std::string& url, const Timeout* timeout) override;
  Status Reload(const Timeout* timeout) override;
  Status Freeze(const Timeout* timeout) override;
  Status Resume(const Timeout* timeout) override;
  Status SendCommand(const std::string& cmd,
                     const base::DictionaryValue& params) override;
  Status SendCommandFromWebSocket(const std::string& cmd,
                                  const base::DictionaryValue& params,
                                  const int client_cmd_id) override;
  Status SendCommandAndGetResult(const std::string& cmd,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value) override;
  Status TraverseHistory(int delta, const Timeout* timeout) override;
  Status EvaluateScript(const std::string& frame,
                        const std::string& function,
                        std::unique_ptr<base::Value>* result) override;
  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override;
  Status CallAsyncFunction(const std::string& frame,
                           const std::string& function,
                           const base::ListValue& args,
                           const base::TimeDelta& timeout,
                           std::unique_ptr<base::Value>* result) override;
  Status CallUserAsyncFunction(const std::string& frame,
                               const std::string& function,
                               const base::ListValue& args,
                               const base::TimeDelta& timeout,
                               std::unique_ptr<base::Value>* result) override;
  Status CallUserSyncScript(const std::string& frame,
                            const std::string& script,
                            const base::ListValue& args,
                            const base::TimeDelta& timeout,
                            std::unique_ptr<base::Value>* result) override;
  Status GetFrameByFunction(const std::string& frame,
                            const std::string& function,
                            const base::ListValue& args,
                            std::string* out_frame) override;
  Status DispatchMouseEvents(const std::list<MouseEvent>& events,
                             const std::string& frame,
                             bool async_dispatch_events = false) override;
  Status DispatchTouchEvent(const TouchEvent& event,
                            bool async_dispatch_events = false) override;
  Status DispatchTouchEvents(const std::list<TouchEvent>& events,
                             bool async_dispatch_events = false) override;
  Status DispatchTouchEventWithMultiPoints(
      const std::list<TouchEvent>& events,
      bool async_dispatch_events = false) override;
  Status DispatchKeyEvents(const std::list<KeyEvent>& events,
                           bool async_dispatch_events = false) override;
  Status GetCookies(std::unique_ptr<base::ListValue>* cookies,
                    const std::string& current_page_url) override;
  Status DeleteCookie(const std::string& name,
                      const std::string& url,
                      const std::string& domain,
                      const std::string& path) override;
  Status AddCookie(const std::string& name,
                   const std::string& url,
                   const std::string& value,
                   const std::string& domain,
                   const std::string& path,
                   bool secure,
                   bool httpOnly,
                   double expiry) override;
  Status WaitForPendingNavigations(const std::string& frame_id,
                                   const Timeout& timeout,
                                   bool stop_load_on_timeout) override;
  Status IsPendingNavigation(const std::string& frame_id,
                             const Timeout* timeout,
                             bool* is_pending) override;
  JavaScriptDialogManager* GetJavaScriptDialogManager() override;
  Status OverrideGeolocation(const Geoposition& geoposition) override;
  Status OverrideNetworkConditions(
      const NetworkConditions& network_conditions) override;
  Status OverrideDownloadDirectoryIfNeeded(
      const std::string& download_directory) override;
  Status CaptureScreenshot(
      std::string* screenshot,
      const base::DictionaryValue& params) override;
  Status SetFileInputFiles(const std::string& frame,
                           const base::DictionaryValue& element,
                           const std::vector<base::FilePath>& files,
                           const bool append) override;
  Status TakeHeapSnapshot(std::unique_ptr<base::Value>* snapshot) override;
  Status StartProfile() override;
  Status EndProfile(std::unique_ptr<base::Value>* profile_data) override;
  Status SynthesizeTapGesture(int x,
                              int y,
                              int tap_count,
                              bool is_long_press) override;
  Status SynthesizeScrollGesture(int x,
                                 int y,
                                 int xoffset,
                                 int yoffset) override;
  bool IsNonBlocking() override;
  bool IsOOPIF(const std::string& frame_id) override;
  FrameTracker* GetFrameTracker() const override;
  std::unique_ptr<base::Value> GetCastSinks() override;
  std::unique_ptr<base::Value> GetCastIssueMessage() override;

 private:
  std::string id_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_STUB_WEB_VIEW_H_
