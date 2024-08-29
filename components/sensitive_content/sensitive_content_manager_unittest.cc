// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sensitive_content/sensitive_content_manager.h"

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/sensitive_content/sensitive_content_client.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace sensitive_content {
namespace {

using autofill::FormData;
using autofill::FormFieldData;
using ::testing::InSequence;
using ::testing::MockFunction;
using LifecycleState = autofill::AutofillDriver::LifecycleState;

class MockSensitiveContentClient : public SensitiveContentClient {
 public:
  MOCK_METHOD(void, SetContentSensitivity, (bool), (override));
};

class SensitiveContentManagerTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    sensitive_content_manager_ = std::make_unique<SensitiveContentManager>(
        web_contents(), &sensitive_content_client_);
  }

  autofill::ContentAutofillDriver* autofill_driver() {
    return autofill_driver_injector_[web_contents()];
  }

  autofill::AutofillManager& autofill_manager() {
    return autofill_driver()->GetAutofillManager();
  }

  MockSensitiveContentClient& sensitive_content_client() {
    return sensitive_content_client_;
  }

  // Creates a `FormData` that is not sensitive and sets its `LocalFrameToken`
  // to the one of the `autofill_driver()`, to mimic production behavior. The
  // proper functioning of
  // `SensitiveContentManager::OnAutofillManagerStateChanged()` depends on
  // `LocalFrameToken`s being set properly.
  FormData CreateNotSensitiveFormData() {
    return autofill::test::CreateFormDataForFrame(
        autofill::test::CreateTestAddressFormData(),
        autofill_driver()->GetFrameToken());
  }

  // Creates a `FormData` that is sensitive and sets its `LocalFrameToken` to
  // the one of the `autofill_driver()`, to mimic production behavior. The
  // proper functioning of
  // `SensitiveContentManager::OnAutofillManagerStateChanged()` depends on
  // `LocalFrameToken`s being set properly.
  FormData CreateSensitiveFormData() {
    return autofill::test::CreateFormDataForFrame(
        autofill::test::CreateTestCreditCardFormData(/*is_https=*/false,
                                                     /*use_month_type=*/false),
        autofill_driver()->GetFrameToken());
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  // Needed to get the driver factory initialized.
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillDriverInjector<autofill::ContentAutofillDriver>
      autofill_driver_injector_;
  MockSensitiveContentClient sensitive_content_client_;
  std::unique_ptr<SensitiveContentManager> sensitive_content_manager_;
};

TEST_F(SensitiveContentManagerTest, AddAndRemoveSensitiveAndNotSensitiveForms) {
  NavigateAndCommit(GURL("https://test.com"));
  FormData not_sensitive_form = CreateNotSensitiveFormData();
  FormData sensitive_form = CreateSensitiveFormData();

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(sensitive_content_client(), SetContentSensitivity).Times(0);
    EXPECT_CALL(check, Call("no sensitive content present"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/true));
    EXPECT_CALL(check, Call("sensitive content present"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/false));
    EXPECT_CALL(check, Call("no sensitive content present anymore"));
    EXPECT_CALL(sensitive_content_client(), SetContentSensitivity).Times(0);
  }

  autofill::TestAutofillManagerWaiter waiter(
      autofill_manager(), {autofill::AutofillManagerEvent::kFormsSeen});
  autofill_manager().OnFormsSeen(/*updated_forms=*/{not_sensitive_form},
                                 /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());
  check.Call("no sensitive content present");

  waiter.Reset();
  autofill_manager().OnFormsSeen(/*updated_forms=*/{sensitive_form},
                                 /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());
  check.Call("sensitive content present");

  waiter.Reset();
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/{},
      /*removed_forms=*/{sensitive_form.global_id()});
  ASSERT_TRUE(waiter.Wait());
  check.Call("no sensitive content present anymore");

  waiter.Reset();
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/{},
      /*removed_forms=*/{not_sensitive_form.global_id()});
  ASSERT_TRUE(waiter.Wait());
}

TEST_F(SensitiveContentManagerTest, AutofillManagerStateChanged) {
  NavigateAndCommit(GURL("https://test.com"));
  FormData not_sensitive_form = CreateNotSensitiveFormData();
  FormData sensitive_form = CreateSensitiveFormData();

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(sensitive_content_client(), SetContentSensitivity).Times(0);
    EXPECT_CALL(check, Call("no sensitive content present so far"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/true));
    EXPECT_CALL(check, Call("sensitive content present now"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/false));
    EXPECT_CALL(check, Call("frame became inactive"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/true));
    EXPECT_CALL(check, Call("frame became active"));
  }

  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kActive);

  autofill::TestAutofillManagerWaiter waiter(
      autofill_manager(), {autofill::AutofillManagerEvent::kFormsSeen});
  autofill_manager().OnFormsSeen(/*updated_forms=*/{not_sensitive_form},
                                 /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());

  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kInactive);
  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kActive);
  check.Call("no sensitive content present so far");

  waiter.Reset();
  autofill_manager().OnFormsSeen(/*updated_forms=*/{sensitive_form},
                                 /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());
  check.Call("sensitive content present now");

  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kInactive);
  check.Call("frame became inactive");
  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kActive);
  check.Call("frame became active");
}

}  // namespace
}  // namespace sensitive_content
