// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_WEB_UI_TEST_HANDLER_H_
#define CHROME_TEST_BASE_ASH_WEB_UI_TEST_HANDLER_H_

#include <optional>
#include <string>

#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class Value;
}  // namespace base

namespace content {
class RenderFrameHost;
}

// This class registers test framework specific handlers on WebUI objects.
class WebUITestHandler {
 public:
  WebUITestHandler();

  WebUITestHandler(const WebUITestHandler&) = delete;
  WebUITestHandler& operator=(const WebUITestHandler&) = delete;

  virtual ~WebUITestHandler();

  // Sends a message through |preload_frame| with the |js_text| to preload at
  // the appropriate time before the onload call is made.
  void PreloadJavaScript(const std::u16string& js_text,
                         content::RenderFrameHost* preload_frame);

  // Runs |js_text| in this object's WebUI frame. Does not wait for any result.
  void RunJavaScript(const std::u16string& js_text);

  // Runs |js_text| in this object's WebUI frame. Waits for result, logging an
  // error message on failure. Returns test pass/fail.
  bool RunJavaScriptTestWithResult(const std::u16string& js_text);

  content::RenderFrameHost* GetRenderFrameHostForTest();

 protected:
  virtual content::WebUI* GetWebUI() = 0;

  // Handles the result of a test. If |error_message| has no value, the test has
  // succeeded.
  void TestComplete(const std::optional<std::string>& error_message);

  // Quits the currently running RunLoop.
  void RunQuitClosure();

 private:
  // Gets the callback that Javascript execution is complete.
  void JavaScriptComplete(base::Value result);

  // Runs a message loop until test finishes. Returns the result of the
  // test.
  bool WaitForResult();

  // Received test pass/fail;
  bool test_done_ = false;

  // Pass fail result of current test.
  bool test_succeeded_ = false;

  // Test code finished trying to execute. This will be set to true when the
  // selected tab is done with this execution request whether it was able to
  // parse/execute the javascript or not.
  bool run_test_done_ = false;

  // Test code was able to execute successfully. This is *NOT* the test
  // pass/fail.
  bool run_test_succeeded_ = false;

  // Quits the currently running RunLoop.
  base::RepeatingClosure quit_closure_;
};

#endif  // CHROME_TEST_BASE_ASH_WEB_UI_TEST_HANDLER_H_
