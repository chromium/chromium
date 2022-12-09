// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

struct BrowserInfo;
struct DeviceMetrics;
class DevToolsClient;
class DownloadDirectoryOverrideManager;
class FrameTracker;
class GeolocationOverrideManager;
class MobileEmulationOverrideManager;
class NetworkConditionsOverrideManager;
class HeapSnapshotTaker;
struct KeyEvent;
struct MouseEvent;
class PageLoadStrategy;
class Status;
class CastTracker;

class WebViewImpl : public WebView {
 public:
  WebViewImpl(const std::string& id,
              const bool w3c_compliant,
              const WebViewImpl* parent,
              const BrowserInfo* browser_info,
              std::unique_ptr<DevToolsClient> client);

  WebViewImpl(const std::string& id,
              const bool w3c_compliant,
              const WebViewImpl* parent,
              const BrowserInfo* browser_info,
              std::unique_ptr<DevToolsClient> client,
              const DeviceMetrics* device_metrics,
              std::string page_load_strategy);
  ~WebViewImpl() override;
  WebViewImpl* CreateChild(const std::string& session_id,
                           const std::string& target_id) const;

  // Overridden from WebView:
  bool IsServiceWorker() const override;
  std::string GetId() override;
  bool WasCrashed() override;
  Status AttachTo(DevToolsClient* parent);
  Status AttachChildView(WebViewImpl* child);
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override;
  Status HandleReceivedEvents() override;
  Status GetUrl(std::string* url) override;
  Status Load(const std::string& url, const Timeout* timeout) override;
  Status Reload(const Timeout* timeout) override;
  Status Freeze(const Timeout* timeout) override;
  Status Resume(const Timeout* timeout) override;
  Status StartBidiServer(std::string bidi_mapper_script) override;
  Status PostBidiCommand(base::Value::Dict command) override;
  Status SendCommand(const std::string& cmd,
                     const base::Value::Dict& params) override;
  Status SendCommandFromWebSocket(const std::string& cmd,
                                  const base::Value::Dict& params,
                                  const int client_cmd_id) override;
  Status SendCommandAndGetResult(const std::string& cmd,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value) override;
  Status TraverseHistory(int delta, const Timeout* timeout) override;
  Status EvaluateScriptWithTimeout(const std::string& frame,
                                   const std::string& expression,
                                   const base::TimeDelta& timeout,
                                   const bool await_promise,
                                   std::unique_ptr<base::Value>* result);
  Status EvaluateScript(const std::string& frame,
                        const std::string& expression,
                        const bool await_promise,
                        std::unique_ptr<base::Value>* result) override;
  Status CallFunctionWithTimeout(const std::string& frame,
                                 const std::string& function,
                                 const base::Value::List& args,
                                 const base::TimeDelta& timeout,
                                 std::unique_ptr<base::Value>* result);
  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::Value::List& args,
                      std::unique_ptr<base::Value>* result) override;
  Status CallAsyncFunction(const std::string& frame,
                           const std::string& function,
                           const base::Value::List& args,
                           const base::TimeDelta& timeout,
                           std::unique_ptr<base::Value>* result) override;
  Status CallUserSyncScript(const std::string& frame,
                            const std::string& script,
                            const base::Value::List& args,
                            const base::TimeDelta& timeout,
                            std::unique_ptr<base::Value>* result) override;
  Status CallUserAsyncFunction(const std::string& frame,
                               const std::string& function,
                               const base::Value::List& args,
                               const base::TimeDelta& timeout,
                               std::unique_ptr<base::Value>* result) override;
  Status GetFrameByFunction(const std::string& frame,
                            const std::string& function,
                            const base::Value::List& args,
                            std::string* out_frame) override;
  Status DispatchMouseEvents(const std::vector<MouseEvent>& events,
                             const std::string& frame,
                             bool async_dispatch_events = false) override;
  Status DispatchTouchEvent(const TouchEvent& event,
                            bool async_dispatch_events = false) override;
  Status DispatchTouchEvents(const std::vector<TouchEvent>& events,
                             bool async_dispatch_events = false) override;
  Status DispatchTouchEventWithMultiPoints(
      const std::vector<TouchEvent>& events,
      bool async_dispatch_events = false) override;
  Status DispatchKeyEvents(const std::vector<KeyEvent>& events,
                           bool async_dispatch_events = false) override;
  Status GetCookies(base::Value* cookies,
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
                   const std::string& same_site,
                   bool secure,
                   bool http_only,
                   double expiry) override;
  Status WaitForPendingNavigations(const std::string& frame_id,
                                   const Timeout& timeout,
                                   bool stop_load_on_timeout) override;
  Status IsPendingNavigation(const Timeout* timeout,
                             bool* is_pending) const override;
  JavaScriptDialogManager* GetJavaScriptDialogManager() override;
  MobileEmulationOverrideManager* GetMobileEmulationOverrideManager()
      const override;
  Status OverrideGeolocation(const Geoposition& geoposition) override;
  Status OverrideNetworkConditions(
      const NetworkConditions& network_conditions) override;
  Status OverrideDownloadDirectoryIfNeeded(
      const std::string& download_directory) override;
  Status CaptureScreenshot(std::string* screenshot,
                           const base::Value::Dict& params) override;
  Status PrintToPDF(const base::Value::Dict& params, std::string* pdf) override;
  Status SetFileInputFiles(const std::string& frame,
                           const base::Value& element,
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
  Status GetBackendNodeIdByElement(const std::string& frame,
                                   const base::Value& element,
                                   int* backend_node_id) override;
  bool IsNonBlocking() const override;
  FrameTracker* GetFrameTracker() const override;
  std::unique_ptr<base::Value> GetCastSinks() override;
  std::unique_ptr<base::Value> GetCastIssueMessage() override;
  void SetFrame(const std::string& new_frame_id) override;

  const WebViewImpl* GetParent() const;
  bool Lock();
  void Unlock();
  bool IsLocked() const;
  void SetDetached();
  bool IsDetached() const;

 private:
  Status CallAsyncFunctionInternal(const std::string& frame,
                                   const std::string& function,
                                   const base::Value::List& args,
                                   bool is_user_supplied,
                                   const base::TimeDelta& timeout,
                                   std::unique_ptr<base::Value>* result);
  Status IsNotPendingNavigation(const std::string& frame_id,
                                const Timeout* timeout,
                                bool* is_not_pending);

  Status InitProfileInternal();
  Status StopProfileInternal();
  Status DispatchTouchEventsForMouseEvents(
      const std::vector<MouseEvent>& events,
      const std::string& frame);

  std::string id_;
  bool w3c_compliant_;
  raw_ptr<const BrowserInfo> browser_info_;
  // Data for WebViewImplHolder to support delayed destruction of WebViewImpl.
  bool is_locked_;
  bool is_detached_;
  raw_ptr<const WebViewImpl> parent_;
  // Many trackers hold pointers to DevToolsClient, so client_ must be declared
  // before the trackers, to ensured trackers are destructed before client_.
  std::unique_ptr<DevToolsClient> client_;
  std::unique_ptr<FrameTracker> frame_tracker_;
  std::unique_ptr<JavaScriptDialogManager> dialog_manager_;
  std::unique_ptr<PageLoadStrategy> navigation_tracker_;
  std::unique_ptr<MobileEmulationOverrideManager>
      mobile_emulation_override_manager_;
  std::unique_ptr<GeolocationOverrideManager> geolocation_override_manager_;
  std::unique_ptr<NetworkConditionsOverrideManager>
      network_conditions_override_manager_;
  std::unique_ptr<DownloadDirectoryOverrideManager>
      download_directory_override_manager_;
  std::unique_ptr<HeapSnapshotTaker> heap_snapshot_taker_;
  std::unique_ptr<CastTracker> cast_tracker_;
  bool is_service_worker_;
};

// Responsible for locking a WebViewImpl and its associated data structure to
// prevent them from being freed which they are still in use.
class WebViewImplHolder {
 public:
  explicit WebViewImplHolder(WebViewImpl* web_view);

  WebViewImplHolder(const WebViewImplHolder&) = delete;
  WebViewImplHolder& operator=(const WebViewImplHolder&) = delete;

  ~WebViewImplHolder();

 private:
  struct Item {
    raw_ptr<WebViewImpl> web_view;
    bool was_locked;
  };
  std::vector<Item> items_;
};

namespace internal {

enum EvaluateScriptReturnType {
  ReturnByValue,
  ReturnByObject
};
Status EvaluateScript(DevToolsClient* client,
                      const std::string& context_id,
                      const std::string& expression,
                      EvaluateScriptReturnType return_type,
                      const base::TimeDelta& timeout,
                      const bool await_promise,
                      base::Value::Dict& result);
Status EvaluateScriptAndGetObject(DevToolsClient* client,
                                  const std::string& context_id,
                                  const std::string& expression,
                                  const base::TimeDelta& timeout,
                                  const bool await_promise,
                                  bool* got_object,
                                  std::string* object_id);
Status EvaluateScriptAndGetValue(DevToolsClient* client,
                                 const std::string& context_id,
                                 const std::string& expression,
                                 const base::TimeDelta& timeout,
                                 const bool await_promise,
                                 std::unique_ptr<base::Value>* result);
Status ParseCallFunctionResult(const base::Value& temp_result,
                               std::unique_ptr<base::Value>* result);
Status GetBackendNodeIdFromFunction(DevToolsClient* client,
                                    const std::string& context_id,
                                    const std::string& function,
                                    const base::Value::List& args,
                                    bool* found_node,
                                    int* backend_node_id,
                                    bool w3c_compliant);
Status GetFrameIdFromFunction(DevToolsClient* client,
                              const std::string& context_id,
                              const std::string& function,
                              const base::Value::List& args,
                              bool* found_node,
                              std::string* frame_id,
                              bool w3c_compliant);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_IMPL_H_
