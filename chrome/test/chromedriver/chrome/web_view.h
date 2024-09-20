// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/values.h"

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

class FedCmTracker;
class FrameTracker;
class MobileEmulationOverrideManager;
class Status;
class Timeout;
struct Geoposition;
struct KeyEvent;
struct MouseEvent;
struct NetworkConditions;
struct TouchEvent;

class WebView {
 public:
  typedef base::RepeatingCallback<Status(bool* is_condition_met)>
      ConditionalFunc;

  virtual ~WebView() = default;

  virtual bool IsServiceWorker() const = 0;

  // Return the id for this WebView.
  virtual std::string GetId() = 0;

  // Return true if the web view was crashed.
  virtual bool WasCrashed() = 0;

  // Handles events until the given function reports the condition is met
  // and there are no more received events to handle. If the given
  // function ever returns an error, returns immediately with the error.
  // If the condition is not met within |timeout|, kTimeout status
  // is returned eventually. If |timeout| is 0, this function will not block.
  virtual Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                                   const Timeout& timeout) = 0;

  // Handles events that have been received but not yet handled.
  virtual Status HandleReceivedEvents() = 0;

  // Get the current URL of the main frame.
  virtual Status GetUrl(std::string* url) = 0;

  // Load a given URL in the main frame.
  virtual Status Load(const std::string& url, const Timeout* timeout) = 0;

  // Reload the current page.
  virtual Status Reload(const Timeout* timeout) = 0;

  // Freeze the current page.
  virtual Status Freeze(const Timeout* timeout) = 0;

  // Resume the current page.
  virtual Status Resume(const Timeout* timeout) = 0;

  virtual Status StartBidiServer(std::string bidi_mapper_string) = 0;

  // Send the BiDi command to the BiDiMapper
  virtual Status PostBidiCommand(base::Value::Dict command) = 0;

  // Send the BiDi command to the BiDiMapper and receive the response
  // Precondition: commdand.Find("id") != nullptr
  // Precondition: commdand.FindString("channel") != nullptr
  virtual Status SendBidiCommand(base::Value::Dict command,
                                 const Timeout& timeout,
                                 base::Value::Dict& response) = 0;

  // Send a command to the DevTools debugger
  virtual Status SendCommand(const std::string& cmd,
                             const base::Value::Dict& params) = 0;

  // Send a command to the DevTools debugger. Received from WebSocket
  virtual Status SendCommandFromWebSocket(const std::string& cmd,
                                          const base::Value::Dict& params,
                                          const int client_cmd_id) = 0;

  // Send a command to the DevTools debugger and wait for the result
  virtual Status SendCommandAndGetResult(
      const std::string& cmd,
      const base::Value::Dict& params,
      std::unique_ptr<base::Value>* value) = 0;

  // Navigate |delta| steps forward in the browser history. A negative value
  // will navigate back in the history. If the delta exceeds the number of items
  // in the browser history, stay on the current page.
  virtual Status TraverseHistory(int delta, const Timeout* timeout) = 0;

  // Evaluates a JavaScript expression in a specified frame and returns
  // the result. |frame| is a frame ID or an empty string for the main frame.
  // If the expression evaluates to a element, it will be bound to a unique ID
  // (per frame) and the ID will be returned.
  // |await_promise| controls awaitPromise parameter for Command
  // send to devtools backend
  // |result| will never be NULL on success.
  virtual Status EvaluateScript(const std::string& frame,
                                const std::string& expression,
                                const bool await_promise,
                                std::unique_ptr<base::Value>* result) = 0;

  // Calls a JavaScript function in a specified frame with the given args and
  // returns the result. |frame| is a frame ID or an empty string for the main
  // frame. |args| may contain IDs that refer to previously returned elements.
  // These will be translated back to their referred objects before invoking the
  // function.
  // |result| will never be NULL on success.
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::Value::List& args,
                              std::unique_ptr<base::Value>* result) = 0;

  // Same as |CallAsyncFunction|, except no additional error callback is passed
  // to the function. Also, |kJavaScriptError| or |kScriptTimeout| is used
  // as the error code instead of |kUnknownError| in appropriate cases.
  // |result| will never be NULL on success.
  virtual Status CallUserAsyncFunction(
      const std::string& frame,
      const std::string& function,
      const base::Value::List& args,
      const base::TimeDelta& timeout,
      std::unique_ptr<base::Value>* result) = 0;

  // Same as |CallFunction|, except |kJavaScriptError| or |kScriptTimeout| is
  // used as the error code instead of |kUnknownError| in appropriate cases, and
  // respects timeout.
  // |result| will never be NULL on success.
  virtual Status CallUserSyncScript(const std::string& frame,
                                    const std::string& script,
                                    const base::Value::List& args,
                                    const base::TimeDelta& timeout,
                                    std::unique_ptr<base::Value>* result) = 0;

  // Gets the frame ID for a frame element returned by invoking the given
  // JavaScript function. |frame| is a frame ID or an empty string for the main
  // frame.
  virtual Status GetFrameByFunction(const std::string& frame,
                                    const std::string& function,
                                    const base::Value::List& args,
                                    std::string* out_frame) = 0;

  // Dispatch a sequence of mouse events.
  virtual Status DispatchMouseEvents(const std::vector<MouseEvent>& events,
                                     const std::string& frame,
                                     bool async_dispatch_events) = 0;

  // Dispatch a single touch event.
  virtual Status DispatchTouchEvent(const TouchEvent& event,
                                    bool async_dispatch_events) = 0;

  // Dispatch a sequence of touch events.
  virtual Status DispatchTouchEvents(const std::vector<TouchEvent>& events,
                                     bool async_dispatch_events) = 0;

  // Dispatch a single touch event with more than one touch point.
  virtual Status DispatchTouchEventWithMultiPoints(
      const std::vector<TouchEvent>& events,
      bool async_dispatch_events) = 0;
  // Dispatch a sequence of key events.
  virtual Status DispatchKeyEvents(const std::vector<KeyEvent>& events,
                                   bool async_dispatch_events) = 0;

  // Return all the cookies visible to the current page.
  virtual Status GetCookies(base::Value* cookies,
                            const std::string& current_page_url) = 0;

  // Delete the cookie with the given name.
  virtual Status DeleteCookie(const std::string& name,
                              const std::string& url,
                              const std::string& domain,
                              const std::string& path) = 0;

  virtual Status AddCookie(const std::string& name,
                           const std::string& url,
                           const std::string& value,
                           const std::string& domain,
                           const std::string& path,
                           const std::string& same_site,
                           bool secure,
                           bool http_only,
                           double expiry) = 0;

  // Waits until all pending navigations have completed in the given frame.
  // If |frame_id| is "", waits for navigations on the main frame.
  // If a modal dialog appears while waiting, kUnexpectedAlertOpen will be
  // returned.
  // If timeout is exceeded, will return a timeout status.
  // If |stop_load_on_timeout| is true, will attempt to stop the page load on
  // timeout before returning the timeout status.
  virtual Status WaitForPendingNavigations(const std::string& frame_id,
                                           const Timeout& timeout,
                                           bool stop_load_on_timeout) = 0;

  // Returns whether the current frame is pending navigation.
  virtual Status IsPendingNavigation(const Timeout* timeout,
                                     bool* is_pending) const = 0;

  // Returns the MobileEmulationOverrideManager.
  virtual MobileEmulationOverrideManager* GetMobileEmulationOverrideManager()
      const = 0;

  // Overrides normal geolocation with a given geoposition.
  virtual Status OverrideGeolocation(const Geoposition& geoposition) = 0;

  // Overrides normal network conditions with given conditions.
  virtual Status OverrideNetworkConditions(
      const NetworkConditions& network_conditions) = 0;

  // Overrides normal download directory with given path.
  virtual Status OverrideDownloadDirectoryIfNeeded(
      const std::string& download_directory) = 0;

  // Captures the visible portions of the web view as a base64-encoded PNG.
  virtual Status CaptureScreenshot(std::string* screenshot,
                                   const base::Value::Dict& params) = 0;

  virtual Status PrintToPDF(const base::Value::Dict& params,
                            std::string* pdf) = 0;

  // Set files in a file input element.
  // |element| is the WebElement JSON Object of the input element.
  virtual Status SetFileInputFiles(const std::string& frame,
                                   const base::Value& element,
                                   const std::vector<base::FilePath>& files,
                                   const bool append) = 0;

  // Take a heap snapshot which can build up a graph of Javascript objects.
  // A raw heap snapshot is in JSON format:
  //  1. A meta data element "snapshot" about how to parse data elements.
  //  2. Data elements: "nodes", "edges", "strings".
  virtual Status TakeHeapSnapshot(std::unique_ptr<base::Value>* snapshot) = 0;

  // Start recording Javascript CPU Profile.
  virtual Status StartProfile() = 0;

  // Stop recording Javascript CPU Profile and returns a graph of
  // CPUProfile objects. The format for the captured profile is defined
  // (by DevTools) in protocol.json.
  virtual Status EndProfile(std::unique_ptr<base::Value>* profile_data) = 0;

  virtual Status SynthesizeTapGesture(int x,
                                      int y,
                                      int tap_count,
                                      bool is_long_press) = 0;

  virtual Status SynthesizeScrollGesture(int x,
                                         int y,
                                         int xoffset,
                                         int yoffset) = 0;

  virtual bool IsNonBlocking() const = 0;

  virtual FrameTracker* GetFrameTracker() const = 0;

  // On success, sets *tracker to the FedCmTracker.
  virtual Status GetFedCmTracker(FedCmTracker** out_tracker) = 0;

  virtual std::unique_ptr<base::Value> GetCastSinks() = 0;

  virtual std::unique_ptr<base::Value> GetCastIssueMessage() = 0;

  virtual void SetFrame(const std::string& new_frame_id) = 0;

  virtual Status GetBackendNodeIdByElement(const std::string& frame,
                                           const base::Value& element,
                                           int* backend_node_id) = 0;

  virtual bool IsDetached() const = 0;

  virtual Status CallFunctionWithTimeout(
      const std::string& frame,
      const std::string& function,
      const base::Value::List& args,
      const base::TimeDelta& timeout,
      std::unique_ptr<base::Value>* result) = 0;

  virtual bool IsDialogOpen() const = 0;

  virtual Status GetDialogMessage(std::string& message) const = 0;

  virtual Status GetTypeOfDialog(std::string& type) const = 0;

  virtual Status HandleDialog(bool accept,
                              const std::optional<std::string>& text) = 0;

  virtual WebView* FindContainerForFrame(const std::string& frame_id) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_H_
