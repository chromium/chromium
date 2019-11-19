// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_ui_test_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using content::RenderViewHost;

WebUITestHandler::WebUITestHandler()
    : test_done_(false),
      test_succeeded_(false),
      run_test_done_(false),
      run_test_succeeded_(false) {}

WebUITestHandler::~WebUITestHandler() {}

void WebUITestHandler::PreloadJavaScript(const base::string16& js_text,
                                         RenderViewHost* preload_host) {
  DCHECK(preload_host);
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  preload_host->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  chrome_render_frame->ExecuteWebUIJavaScript(js_text);
}

void WebUITestHandler::RunJavaScript(const base::string16& js_text) {
  GetWebUI()->GetWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
      js_text, base::NullCallback());
}

bool WebUITestHandler::RunJavaScriptTestWithResult(
    const base::string16& js_text) {
  test_succeeded_ = false;
  run_test_succeeded_ = false;
  content::RenderFrameHost* frame =
      GetWebUI()->GetWebContents()->GetMainFrame();
  frame->ExecuteJavaScriptForTests(
      js_text, base::BindOnce(&WebUITestHandler::JavaScriptComplete,
                              base::Unretained(this)));
  return WaitForResult();
}

void WebUITestHandler::TestComplete(
    const base::Optional<std::string>& error_message) {
  // To ensure this gets done, do this before ASSERT* calls.
  RunQuitClosure();
  SCOPED_TRACE("WebUITestHandler::TestComplete");

  ASSERT_FALSE(test_done_);
  test_done_ = true;
  test_succeeded_ = !error_message.has_value();

  if (!test_succeeded_)
    LOG(ERROR) << *error_message;
}

void WebUITestHandler::RunQuitClosure() {
  quit_closure_.Run();
}

void WebUITestHandler::JavaScriptComplete(base::Value result) {
  // To ensure this gets done, do this before ASSERT* calls.
  RunQuitClosure();

  SCOPED_TRACE("WebUITestHandler::JavaScriptComplete");

  EXPECT_FALSE(run_test_done_);
  run_test_done_ = true;
  run_test_succeeded_ = false;

  ASSERT_TRUE(result.GetAsBoolean(&run_test_succeeded_));
}

bool WebUITestHandler::WaitForResult() {
  SCOPED_TRACE("WebUITestHandler::WaitForResult");
  test_done_ = false;
  run_test_done_ = false;

  // Either sync test completion or the testDone() will cause message loop
  // to quit.
  {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitWhenIdleClosure();
    content::RunThisRunLoop(&run_loop);
  }

  // Run a second message loop when not |run_test_done_| so that the sync test
  // completes, or |run_test_succeeded_| but not |test_done_| so async tests
  // complete.
  if (!run_test_done_ || (run_test_succeeded_ && !test_done_)) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitWhenIdleClosure();
    content::RunThisRunLoop(&run_loop);
  }

  // To succeed the test must execute as well as pass the test.
  return run_test_succeeded_ && test_succeeded_;
}
