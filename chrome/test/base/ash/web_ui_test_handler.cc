// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/web_ui_test_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

WebUITestHandler::WebUITestHandler() = default;
WebUITestHandler::~WebUITestHandler() = default;

void WebUITestHandler::PreloadJavaScript(
    const std::u16string& js_text,
    content::RenderFrameHost* preload_frame) {
  DCHECK(preload_frame);
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  preload_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  chrome_render_frame->ExecuteWebUIJavaScript(js_text);
}

void WebUITestHandler::RunJavaScript(const std::u16string& js_text) {
  GetRenderFrameHostForTest()->ExecuteJavaScriptForTests(
      js_text, base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
}

bool WebUITestHandler::RunJavaScriptTestWithResult(
    const std::u16string& js_text) {
  test_succeeded_ = false;
  run_test_succeeded_ = false;
  GetRenderFrameHostForTest()->ExecuteJavaScriptForTests(
      js_text,
      base::BindOnce(&WebUITestHandler::JavaScriptComplete,
                     base::Unretained(this)),
      content::ISOLATED_WORLD_ID_GLOBAL);
  return WaitForResult();
}

content::RenderFrameHost* WebUITestHandler::GetRenderFrameHostForTest() {
  return GetWebUI()->GetWebContents()->GetPrimaryMainFrame();
}

void WebUITestHandler::TestComplete(
    const std::optional<std::string>& error_message) {
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

  ASSERT_TRUE(result.is_bool());
  run_test_succeeded_ = result.GetBool();
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
    run_loop.Run();
  }

  // Run a second message loop when not |run_test_done_| so that the sync test
  // completes, or |run_test_succeeded_| but not |test_done_| so async tests
  // complete.
  if (!run_test_done_ || (run_test_succeeded_ && !test_done_)) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitWhenIdleClosure();
    run_loop.Run();
  }

  // To succeed the test must execute as well as pass the test.
  return run_test_succeeded_ && test_succeeded_;
}
