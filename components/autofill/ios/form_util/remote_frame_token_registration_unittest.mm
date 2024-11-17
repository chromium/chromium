// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <optional>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/new_frame_catcher.h"
#import "components/autofill/ios/browser/test_autofill_java_script_feature_container.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/form_util/autofill_renderer_id_java_script_feature.h"
#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"

using autofill::test::NewFrameCatcher;
using base::test::ios::kWaitForJSCompletionTimeout;
using web::WebFrame;

namespace autofill {

// Test fixture for verifying the registration of remote frame tokens associated
// to frame IDs in the frames registrar.
class RemoteFrameTokenRegistrationTest : public web::WebTestWithWebState {
 public:
  RemoteFrameTokenRegistrationTest()
      : web::WebTestWithWebState(std::make_unique<web::FakeWebClient>()),
        feature_list_(kAutofillIsolatedWorldForJavascriptIos) {
    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(GetWebClient());
    web_client->SetJavaScriptFeatures({
        form_handlers_java_script_feature(),
    });
  }

 protected:
  web::WebFramesManager* web_frames_manager() {
    return js_feature_container.form_handlers_java_script_feature()
        ->GetWebFramesManager(web_state());
  }

  FormHandlersJavaScriptFeature* form_handlers_java_script_feature() {
    return js_feature_container.form_handlers_java_script_feature();
  }

  ChildFrameRegistrar* frame_registrar() {
    return ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  }

  // Wait for a new frame to become available.
  WebFrame* WaitForNewFrame(
      std::unique_ptr<NewFrameCatcher> new_frame_catcher) {
    NewFrameCatcher* catcher_ptr = new_frame_catcher.get();
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        kWaitForJSCompletionTimeout, ^bool {
          return catcher_ptr->latest_new_frame() != nullptr;
        }));
    return new_frame_catcher->latest_new_frame();
  }

  // Reads the remote frame token stored in the DOM of `frame`;
  std::optional<RemoteFrameToken> GetRemoteFrameToken(WebFrame* frame) {
    __block bool done = false;
    __block std::string remote_frame_token;
    frame->ExecuteJavaScript(
        u"document.documentElement.getAttribute('__gChrome_remoteFrameToken')",
        base::BindOnce(^(const base::Value* result) {
          if (result && result->is_string()) {
            remote_frame_token = result->GetString();
          }
          done = true;
        }));

    bool execution_complete = base::test::ios::WaitUntilConditionOrTimeout(
        kWaitForJSCompletionTimeout, ^() {
          return done;
        });

    if (execution_complete) {
      if (std::optional<base::UnguessableToken> remote =
              DeserializeJavaScriptFrameId(remote_frame_token)) {
        return RemoteFrameToken(*remote);
      }
    }

    return std::nullopt;
  }

  // Finds the local frame token associated to `remote_frame_token` in the
  // frames registrar.
  std::optional<LocalFrameToken> LookupLocalFrameToken(
      RemoteFrameToken remote_frame_token) {
    return frame_registrar()->LookupChildFrame(remote_frame_token);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  //  Test instances of JavaScriptFeature's that are injected in a different
  //  content world depending on kAutofillIsolatedWorldForJavascriptIos.
  //  TODO(crbug.com/359538514): Remove this variable and use
  //  FormHandlersJavaScriptFeature::GetInstance() once Autofill in the isolated
  //  world is launched.
  TestAutofillJavaScriptFeatureContainer js_feature_container;
};

// Verifies that the main frame registers a remote frame token associated to its
// frame id in the frames registrar.
TEST_F(RemoteFrameTokenRegistrationTest, MainFrameRegistersItself) {
  std::unique_ptr<NewFrameCatcher> catcher =
      std::make_unique<NewFrameCatcher>(web_frames_manager());
  LoadHtml(@"<html><body>Test</body></html>");
  WebFrame* frame = WaitForNewFrame(std::move(catcher));
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->IsMainFrame());

  std::optional<RemoteFrameToken> remote_frame_token =
      GetRemoteFrameToken(frame);
  ASSERT_TRUE(remote_frame_token);

  // Verify that the remote frame token was registered to the frame id.
  std::optional<LocalFrameToken> local_frame_token =
      LookupLocalFrameToken(*remote_frame_token);
  ASSERT_TRUE(local_frame_token);

  EXPECT_THAT(frame->GetFrameId(),
              testing::StrCaseEq(local_frame_token->ToString()));
}

// Validates that a child frame registers a remote frame token associated to its
// frame id.
TEST_F(RemoteFrameTokenRegistrationTest, ChildFrameRegistersItself) {
  std::unique_ptr<NewFrameCatcher> catcher =
      std::make_unique<NewFrameCatcher>(web_frames_manager());
  LoadHtml(@"<html><body>Test</body></html>");
  WebFrame* main_frame = WaitForNewFrame(std::move(catcher));
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(main_frame->IsMainFrame());

  catcher = std::make_unique<NewFrameCatcher>(web_frames_manager());

  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"const newFrame = document.createElement('iframe');newFrame.id = "
      @"'frame1';document.body.appendChild(newFrame);",
      form_handlers_java_script_feature());

  WebFrame* child_frame = WaitForNewFrame(std::move(catcher));
  ASSERT_TRUE(child_frame);
  ASSERT_FALSE(child_frame->IsMainFrame());

  std::optional<RemoteFrameToken> main_remote_token =
      GetRemoteFrameToken(main_frame);
  std::optional<RemoteFrameToken> child_remote_token =
      GetRemoteFrameToken(child_frame);
  ASSERT_TRUE(main_remote_token);
  ASSERT_TRUE(child_remote_token);

  // Verify that each frame registered a remote token associated to their frame
  // IDs.
  std::optional<LocalFrameToken> main_local_token =
      LookupLocalFrameToken(*main_remote_token);
  std::optional<LocalFrameToken> child_local_token =
      LookupLocalFrameToken(*child_remote_token);
  ASSERT_TRUE(main_local_token);
  ASSERT_TRUE(child_local_token);

  EXPECT_THAT(main_frame->GetFrameId(),
              testing::StrCaseEq(main_local_token->ToString()));
  EXPECT_THAT(child_frame->GetFrameId(),
              testing::StrCaseEq(child_local_token->ToString()));
}

}  // namespace autofill
