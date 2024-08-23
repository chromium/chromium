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

  FormData CreateNotSensitiveFormData() {
    return autofill::test::CreateTestAddressFormData();
  }

  FormData CreateSensitiveFormData() {
    FormData sensitive_form = autofill::test::CreateTestAddressFormData();
    sensitive_form.set_fields({autofill::test::CreateTestFormField(
        "Credit card number", "Credit card number", "",
        autofill::FormControlType::kInputText, "cc-number")});
    return sensitive_form;
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

  testing::MockFunction<void(std::string_view)> check;
  {
    testing::InSequence s;
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

}  // namespace
}  // namespace sensitive_content
