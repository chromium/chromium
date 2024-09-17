// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/mobile_device.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

struct BrowserInfo;
class DevToolsClient;
class DownloadDirectoryOverrideManager;
class FedCmTracker;
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
  static std::unique_ptr<WebViewImpl> CreateServiceWorkerWebView(
      const std::string& id,
      const bool w3c_compliant,
      const BrowserInfo* browser_info,
      std::unique_ptr<DevToolsClient> client);
  static std::unique_ptr<WebViewImpl> CreateTopLevelWebView(
      const std::string& id,
      const bool w3c_compliant,
      const BrowserInfo* browser_info,
      std::unique_ptr<DevToolsClient> client,
      std::optional<MobileDevice> mobile_device,
      std::string page_load_strategy,
      bool autoaccept_beforeunload);
  WebViewImpl(const std::string& id,
              const bool w3c_compliant,
              const WebViewImpl* parent,
              const BrowserInfo* browser_info,
              std::unique_ptr<DevToolsClient> client,
              std::optional<MobileDevice> mobile_device,
              std::string page_load_strategy,
              bool autoaccept_beforeunload);
  ~WebViewImpl() override;
  std::unique_ptr<WebViewImpl> CreateChild(const std::string& session_id,
                                           const std::string& target_id) const;

  // Overridden from WebView:
  bool IsServiceWorker() const override;
  std::string GetId() override;
  bool WasCrashed() override;
  Status AttachTo(DevToolsClient* root_client);
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
  Status SendBidiCommand(base::Value::Dict command,
                         const Timeout& timeout,
                         base::Value::Dict& response) override;
  Status SendCommand(const std::string& cmd,
                     const base::Value::Dict& params) override;
  Status SendCommandFromWebSocket(const std::string& cmd,
                                  const base::Value::Dict& params,
                                  const int client_cmd_id) override;
  Status SendCommandAndGetResult(const std::string& cmd,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value) override;
  Status TraverseHistory(int delta, const Timeout* timeout) override;
  Status EvaluateScript(const std::string& frame,
                        const std::string& expression,
                        const bool await_promise,
                        std::unique_ptr<base::Value>* result) override;
  Status CallFunctionWithTimeout(const std::string& frame,
                                 const std::string& function,
                                 const base::Value::List& args,
                                 const base::TimeDelta& timeout,
                                 std::unique_ptr<base::Value>* result) override;
  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::Value::List& args,
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
                             bool async_dispatch_events) override;
  Status DispatchTouchEvent(const TouchEvent& event,
                            bool async_dispatch_events) override;
  Status DispatchTouchEvents(const std::vector<TouchEvent>& events,
                             bool async_dispatch_events) override;
  Status DispatchTouchEventWithMultiPoints(
      const std::vector<TouchEvent>& events,
      bool async_dispatch_events) override;
  Status DispatchKeyEvents(const std::vector<KeyEvent>& events,
                           bool async_dispatch_events) override;
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
  Status GetFedCmTracker(FedCmTracker** out_tracker) override;
  FrameTracker* GetFrameTracker() const override;
  std::unique_ptr<base::Value> GetCastSinks() override;
  std::unique_ptr<base::Value> GetCastIssueMessage() override;
  void SetFrame(const std::string& new_frame_id) override;

  const WebViewImpl* GetParent() const;
  bool Lock();
  void Unlock();
  bool IsLocked() const;
  void SetDetached();
  bool IsDetached() const override;

  bool IsDialogOpen() const override;
  Status GetDialogMessage(std::string& message) const override;
  Status GetTypeOfDialog(std::string& type) const override;
  Status HandleDialog(bool accept,
                      const std::optional<std::string>& text) override;

  WebView* FindContainerForFrame(const std::string& frame_id) override;

 protected:
  WebViewImpl(const std::string& id,
              const bool w3c_compliant,
              const WebViewImpl* parent,
              const BrowserInfo* browser_info,
              std::unique_ptr<DevToolsClient> client);

 private:
  WebView* GetTargetForFrame(const std::string& frame);
  Status GetLoaderId(const std::string& frame_id,
                     const Timeout& timeout,
                     std::string& loader_id);
  Status CallFunctionWithTimeoutInternal(std::string frame,
                                         std::string function,
                                         base::Value::List args,
                                         const base::TimeDelta& timeout,
                                         std::unique_ptr<base::Value>* result);
  Status CallAsyncFunctionInternal(const std::string& frame,
                                   const std::string& function,
                                   const base::Value::List& args,
                                   const base::TimeDelta& timeout,
                                   std::unique_ptr<base::Value>* result);
  Status IsNotPendingNavigation(const std::string& frame_id,
                                const Timeout* timeout,
                                bool* is_not_pending);
  Status ResolveElementReferencesInPlace(const std::string& expected_frame_id,
                                         const std::string& context_id,
                                         const std::string& object_group_name,
                                         const std::string& expected_loader_id,
                                         bool w3c_compliant,
                                         const Timeout& timeout,
                                         base::Value& arg,
                                         base::Value::List& nodes);
  Status ResolveElementReferencesInPlace(const std::string& expected_frame_id,
                                         const std::string& context_id,
                                         const std::string& object_group_name,
                                         const std::string& expected_loader_id,
                                         bool w3c_compliant,
                                         const Timeout& timeout,
                                         base::Value::Dict& arg_dict,
                                         base::Value::List& nodes);
  Status ResolveElementReferencesInPlace(const std::string& expected_frame_id,
                                         const std::string& context_id,
                                         const std::string& object_group_name,
                                         const std::string& expected_loader_id,
                                         bool w3c_compliant,
                                         const Timeout& timeout,
                                         base::Value::List& arg_list,
                                         base::Value::List& nodes);
  Status CreateElementReferences(const std::string& frame_id,
                                 const std::string& loader_id,
                                 const base::Value::List& nodes,
                                 base::Value& res);

  Status InitProfileInternal();
  Status StopProfileInternal();
  Status DispatchTouchEventsForMouseEvents(
      const std::vector<MouseEvent>& events,
      const std::string& frame);

  std::unique_ptr<PageLoadStrategy> CreatePageLoadStrategy(
      const std::string& strategy);

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
  std::unique_ptr<FedCmTracker> fedcm_tracker_;
  bool is_service_worker_;
  bool autoaccept_beforeunload_ = false;
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
Status EvaluateScript(DevToolsClient* client,
                      const std::string& context_id,
                      const std::string& expression,
                      const base::TimeDelta& timeout,
                      const bool await_promise,
                      base::Value::Dict& result);
Status EvaluateScriptAndGetValue(DevToolsClient* client,
                                 const std::string& context_id,
                                 const std::string& expression,
                                 const base::TimeDelta& timeout,
                                 const bool await_promise,
                                 std::unique_ptr<base::Value>* result);
Status ParseCallFunctionResult(const base::Value& temp_result,
                               std::unique_ptr<base::Value>* result);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_IMPL_H_
