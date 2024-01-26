// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <vector>

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/test_autofill_client.h"
#import "components/autofill/core/browser/test_autofill_manager_waiter.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gtest/include/gtest/gtest.h"

using testing::IsTrue;
using testing::VariantWith;

namespace autofill {

// Version of AutofillManager that caches the FormData it receives so we can
// examine them. The public API deals with FormStructure, the post-parsing
// data structure, but we want to intercept the FormData and ensure we're
// providing the right inputs to the parsing process.
class TestAutofillManager : public BrowserAutofillManager {
 public:
  TestAutofillManager(AutofillDriverIOS* driver, AutofillClient* client)
      : BrowserAutofillManager(driver, client, "en-US") {}

  [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
      int min_num_awaited_calls) {
    return forms_seen_waiter_.Wait(min_num_awaited_calls);
  }

  void OnFormsSeen(const std::vector<FormData>& updated_forms,
                   const std::vector<FormGlobalId>& removed_forms) override {
    for (const FormData& form : updated_forms) {
      seen_forms_.push_back(form);
    }
    BrowserAutofillManager::OnFormsSeen(updated_forms, removed_forms);
  }

  const std::vector<FormData>& seen_forms() { return seen_forms_; }

  void ResetTestState() {
    seen_forms_.clear();
    forms_seen_waiter_.Reset();
  }

 private:
  std::vector<FormData> seen_forms_;

  TestAutofillManagerWaiter forms_seen_waiter_{
      *this,
      {AutofillManagerEvent::kFormsSeen}};
};

class AutofillAcrossIframesTest : public AutofillTestWithWebState {
 public:
  AutofillAcrossIframesTest()
      : AutofillTestWithWebState(std::make_unique<web::FakeWebClient>()),
        feature_list_(features::kAutofillAcrossIframesIos) {}

  void SetUp() override {
    AutofillTestWithWebState::SetUp();

    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(GetWebClient());
    web_client->SetJavaScriptFeatures(
        {AutofillJavaScriptFeature::GetInstance(),
         FormUtilJavaScriptFeature::GetInstance(),
         FormHandlersJavaScriptFeature::GetInstance()});

    autofill_manager_injector_ =
        std::make_unique<TestAutofillManagerInjector<TestAutofillManager>>(
            web_state());

    // AutofillAgent init crashes without this.
    UniqueIDDataTabHelper::CreateForWebState(web_state());

    // We need an AutofillAgent to exist or else the form will never get parsed.
    prefs_ = autofill::test::PrefServiceForTesting();
    autofill_agent_ = [[AutofillAgent alloc] initWithPrefService:prefs_.get()
                                                        webState:web_state()];

    // Driver factory needs to exist before any call to
    // `AutofillDriverIOS::FromWebStateAndWebFrame`, or we crash.
    autofill::AutofillDriverIOSFactory::CreateForWebState(
        web_state(), &autofill_client_, /*bridge=*/autofill_agent_,
        /*locale=*/"en");
  }

  web::WebFrame* WaitForMainFrame() {
    __block web::WebFrame* main_frame = nullptr;
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool {
          web::WebFramesManager* frames_manager =
              autofill::FormUtilJavaScriptFeature::GetInstance()
                  ->GetWebFramesManager(web_state());
          main_frame = frames_manager->GetMainWebFrame();
          return main_frame != nullptr;
        }));
    return main_frame;
  }

  AutofillDriverIOS* main_frame_driver() {
    return AutofillDriverIOS::FromWebStateAndWebFrame(web_state(),
                                                      WaitForMainFrame());
  }

  TestAutofillManager& main_frame_manager() {
    return static_cast<TestAutofillManager&>(
        main_frame_driver()->GetAutofillManager());
  }

  // Functions for setting up the pages to be loaded. Tests should call one or
  // more of the `Add*` functions, then call `StartTestServerAndLoad`.

  // Adds an iframe loading `path` to the main frame's HTML, and registers a
  // handler on the test server to return `contents` when `path` is requested.
  void AddIframe(std::string path, std::string contents) {
    main_frame_html_ += "<iframe src=\"/" + path + "\"></iframe>";
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/" + path,
        base::BindRepeating(&testing::HandlePageWithHtml, contents)));
  }

  // Adds an input of type `type` with placeholder `ph` to the main frame's
  // HTML.
  void AddInput(std::string type, std::string ph) {
    main_frame_html_ +=
        "<input type=\"" + type + "\" placeholder =\"" + ph + "\">";
  }

  // Starts the test server and loads a page containing `main_frame_html_` in
  // the main frame.
  void StartTestServerAndLoad() {
    main_frame_html_ += "</form></body>";
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/testpage",
        base::BindRepeating(&testing::HandlePageWithHtml, main_frame_html_)));
    ASSERT_TRUE(test_server_.Start());
    web::test::LoadUrl(web_state(), test_server_.GetURL("/testpage"));
  }

  std::unique_ptr<TestAutofillManagerInjector<TestAutofillManager>>
      autofill_manager_injector_;
  std::unique_ptr<PrefService> prefs_;
  autofill::TestAutofillClient autofill_client_;
  AutofillAgent* autofill_agent_;
  base::test::ScopedFeatureList feature_list_;

  net::EmbeddedTestServer test_server_;
  std::string main_frame_html_ = "<body><form>";
};

// If a page has no child frames, the corresponding field in the saved form
// structure should be empty.
TEST_F(AutofillAcrossIframesTest, NoChildFrames) {
  AddInput("text", "name");
  AddInput("text", "address");
  StartTestServerAndLoad();

  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);

  const FormData& form = main_frame_manager().seen_forms()[0];
  EXPECT_EQ(form.child_frames.size(), 0u);

  // The main frame driver should have the correct local frame token set even
  // without any child frames.
  LocalFrameToken token = main_frame_driver()->GetFrameToken();
  ASSERT_TRUE(token);
  web::WebFramesManager* frames_manager =
      FormUtilJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state());
  ASSERT_TRUE(frames_manager);
  web::WebFrame* frame = frames_manager->GetFrameWithId(token.ToString());
  EXPECT_EQ(frame, main_frame_driver()->web_frame());
}

// Ensure that child frames are assigned a token during form extraction, are
// registered under that token with the registrar, and can be found in the
// WebFramesManager using the frame ID provided by the registrar.
TEST_F(AutofillAcrossIframesTest, WithChildFrames) {
  AddIframe("cf1", "child frame 1");
  AddInput("text", "name");
  AddIframe("cf2", "child frame 2");
  AddInput("text", "address");
  StartTestServerAndLoad();

  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);

  const FormData& form = main_frame_manager().seen_forms()[0];
  ASSERT_EQ(form.child_frames.size(), 2u);

  FrameTokenWithPredecessor remote_token1 = form.child_frames[0];
  FrameTokenWithPredecessor remote_token2 = form.child_frames[1];

  // Verify that tokens hold the right alternative, and the token objects are
  // valid (the bool cast checks this).
  EXPECT_THAT(remote_token1.token, VariantWith<RemoteFrameToken>(IsTrue()));
  EXPECT_THAT(remote_token2.token, VariantWith<RemoteFrameToken>(IsTrue()));

  auto* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  ASSERT_TRUE(registrar);

  // Get the frame tokens from the registrar. Wrap this in a block because the
  // registrar receives these from each frame in a separate JS message.
  __block absl::optional<LocalFrameToken> local_token1, local_token2;
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        local_token1 = registrar->LookupChildFrame(
            absl::get<RemoteFrameToken>(remote_token1.token));
        local_token2 = registrar->LookupChildFrame(
            absl::get<RemoteFrameToken>(remote_token2.token));
        return local_token1.has_value() && local_token2.has_value();
      }));

  web::WebFramesManager* frames_manager =
      FormUtilJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state());
  ASSERT_TRUE(frames_manager);

  web::WebFrame* frame1 =
      frames_manager->GetFrameWithId(local_token1->ToString());
  EXPECT_TRUE(frame1);

  web::WebFrame* frame2 =
      frames_manager->GetFrameWithId(local_token2->ToString());
  EXPECT_TRUE(frame2);

  // TODO(crbug.com/1440471): Check contents of frames to make sure they're the
  // right ones.

  // Also check that data relating to the frame was properly set on the form-
  // and field-level data when extracted.
  ASSERT_TRUE(form.host_frame);
  web::WebFrame* main_frame_from_form_data =
      frames_manager->GetFrameWithId(form.host_frame.ToString());
  ASSERT_TRUE(main_frame_from_form_data);
  EXPECT_TRUE(main_frame_from_form_data->IsMainFrame());

  FormSignature form_signature = CalculateFormSignature(form);

  EXPECT_EQ(form.fields.size(), 2u);
  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(field.host_frame, form.host_frame);
    EXPECT_EQ(field.host_form_id, form.unique_renderer_id);
    EXPECT_EQ(field.origin, url::Origin::Create(form.url));
    EXPECT_EQ(field.host_form_signature, form_signature);
  }
}

// Largely repeats `WithChildFrames` above, but exercises the Resolve method on
// AutofillDriverIOS.
TEST_F(AutofillAcrossIframesTest, Resolve) {
  AddIframe("cf1", "child frame 1");
  AddInput("text", "name");
  StartTestServerAndLoad();

  // Wait for a form with a child frame, and grab its remote token.
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);
  const FormData& form = main_frame_manager().seen_forms()[0];
  ASSERT_EQ(form.child_frames.size(), 1u);
  FrameTokenWithPredecessor remote_token = form.child_frames[0];
  EXPECT_THAT(remote_token.token, VariantWith<RemoteFrameToken>(IsTrue()));

  // Wait for the child frame to register itself.
  auto* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  ASSERT_TRUE(registrar);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return registrar
            ->LookupChildFrame(absl::get<RemoteFrameToken>(remote_token.token))
            .has_value();
      }));

  // Verify that resolving the registered remote token returns a valid local
  // token that corresponds to a known frame.
  std::optional<LocalFrameToken> local_token =
      main_frame_driver()->Resolve(remote_token.token);
  ASSERT_TRUE(local_token.has_value());
  web::WebFramesManager* frames_manager =
      FormUtilJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state());
  ASSERT_TRUE(frames_manager);
  EXPECT_TRUE(frames_manager->GetFrameWithId(local_token->ToString()));

  // Verify that resolving a local token is an identity operation.
  EXPECT_EQ(local_token, main_frame_driver()->Resolve(*local_token));

  // Verify that resolving a made-up remote token returns nullopt.
  RemoteFrameToken junk_remote_token =
      RemoteFrameToken(base::UnguessableToken::Create());
  std::optional<LocalFrameToken> shouldnt_exist =
      main_frame_driver()->Resolve(junk_remote_token);
  EXPECT_FALSE(shouldnt_exist.has_value());
}

TEST_F(AutofillAcrossIframesTest, SetAndGetParent) {
  AddIframe("cf1", "child frame 1");
  AddInput("text", "name");
  StartTestServerAndLoad();

  // Wait for a form with a child frame, and grab its remote token.
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);
  const FormData& form = main_frame_manager().seen_forms()[0];
  ASSERT_EQ(form.child_frames.size(), 1u);
  FrameTokenWithPredecessor remote_token = form.child_frames[0];
  EXPECT_THAT(remote_token.token, VariantWith<RemoteFrameToken>(IsTrue()));

  // Wait for the child frame to register itself.
  auto* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  ASSERT_TRUE(registrar);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return registrar
            ->LookupChildFrame(absl::get<RemoteFrameToken>(remote_token.token))
            .has_value();
      }));

  // The main frame shouldn't have a parent – it's the root.
  EXPECT_FALSE(main_frame_driver()->GetParent());

  // The child frame should have the main frame as its parent.
  std::optional<LocalFrameToken> local_token =
      main_frame_driver()->Resolve(remote_token.token);
  ASSERT_TRUE(local_token);
  auto* child_frame_driver = AutofillDriverIOS::FromWebStateAndLocalFrameToken(
      web_state(), *local_token);
  ASSERT_TRUE(child_frame_driver);
  EXPECT_EQ(main_frame_driver(), child_frame_driver->GetParent());
}

TEST_F(AutofillAcrossIframesTest, TriggerExtractionInFrame) {
  AddInput("text", "name");
  AddIframe("cf1", "<form><input id='address'></input></form>");
  StartTestServerAndLoad();

  web::WebFramesManager* frames_manager =
      FormUtilJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state());
  ASSERT_TRUE(frames_manager);

  // Wait for the main frame and the child frame to be known to the
  // WebFramesManager.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return frames_manager->GetAllWebFrames().size() == 2;
      }));

  for (web::WebFrame* frame : frames_manager->GetAllWebFrames()) {
    auto* driver =
        AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), frame);
    auto& manager =
        static_cast<TestAutofillManager&>(driver->GetAutofillManager());

    // Extraction will have triggered on page load. Wait for this to complete.
    EXPECT_TRUE(manager.WaitForFormsSeen(1));
    manager.ResetTestState();

    // Manually retrigger extraction, and wait for a fresh FormsSeen event.
    driver->TriggerFormExtractionInDriverFrame();
    EXPECT_TRUE(manager.WaitForFormsSeen(1));
  }
}

// Ensure that disabling the feature actually disables the feature.
TEST_F(AutofillAcrossIframesTest, FeatureDisabled) {
  base::test::ScopedFeatureList disable;
  disable.InitAndDisableFeature(features::kAutofillAcrossIframesIos);

  AddIframe("cf1", "child frame 1");
  AddInput("text", "name");
  AddIframe("cf2", "child frame 2");
  AddInput("text", "address");
  StartTestServerAndLoad();

  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);

  const FormData& form = main_frame_manager().seen_forms()[0];
  EXPECT_EQ(form.child_frames.size(), 0u);

  EXPECT_FALSE(
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state()));
}

}  // namespace autofill
