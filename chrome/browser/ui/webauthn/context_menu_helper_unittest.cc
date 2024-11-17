// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/context_menu_helper.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/web_contents_user_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using webauthn::IsPasskeyFromAnotherDeviceContextMenuEnabled;

constexpr uint64_t kFormRendererId = 1;
constexpr uint64_t kFieldRendererId = 1;

class MockBrowserAutofillManager : public autofill::TestBrowserAutofillManager {
 public:
  using autofill::TestBrowserAutofillManager::TestBrowserAutofillManager;
  MOCK_METHOD(autofill::FormStructure*,
              FindCachedFormById,
              (autofill::FormGlobalId),
              (const override));
};

class MockAutofillDriver : public autofill::ContentAutofillDriver {
 public:
  using ContentAutofillDriver::ContentAutofillDriver;

  MOCK_METHOD(autofill::AutofillManager&, GetAutofillManager, (), (override));
};

class ContextMenuHelperBaseTest : public ChromeRenderViewHostTestHarness {
 public:
  ContextMenuHelperBaseTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }
};

TEST_F(ContextMenuHelperBaseTest,
       IsPasskeyFromAnotherDeviceContextMenuEnabled_NoDriver) {
  EXPECT_FALSE(IsPasskeyFromAnotherDeviceContextMenuEnabled(
      main_rfh(), kFormRendererId, kFieldRendererId));
}

class ContextMenuHelperWithAfTest : public ContextMenuHelperBaseTest {
 public:
  ContextMenuHelperWithAfTest() = default;

  void SetUp() override {
    ContextMenuHelperBaseTest::SetUp();

    NavigateAndCommit(GURL("about:blank"));
  }

  autofill::FormData CreateFormWithSingleField(bool is_webauthn = false) {
    autofill::FormFieldData field = autofill::test::CreateTestFormField(
        /*label=*/"label", /*name=*/"name",
        /*value=*/"", autofill::FormControlType::kInputText,
        /*autocomplete=*/is_webauthn ? "webauthn" : "");
    field.set_host_frame(autofill_driver()->GetFrameToken());

    autofill::FormData form;
    form.set_renderer_id(autofill::test::MakeFormRendererId());
    form.set_url(GURL("https://uzgunKartal1903.com"));
    form.set_fields({field});
    return form;
  }

  void NotifyFormManagerAndWait(autofill::FormData form) {
    autofill::TestAutofillManagerWaiter waiter(
        autofill_manager(), {autofill::AutofillManagerEvent::kFormsSeen});
    autofill_manager().OnFormsSeen({form}, {});
    ASSERT_TRUE(waiter.Wait());
  }

  autofill::TestContentAutofillDriver* autofill_driver() {
    return af_driver_injector_[main_rfh()];
  }

  autofill::AutofillManager& autofill_manager() {
    return autofill_driver()->GetAutofillManager();
  }

 private:
  autofill::test::AutofillUnitTestEnvironment test_environment_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      af_client_injector;
  autofill::TestAutofillDriverInjector<autofill::TestContentAutofillDriver>
      af_driver_injector_;
  autofill::TestAutofillManagerInjector<MockBrowserAutofillManager>
      af_manager_injector_;
};

TEST_F(ContextMenuHelperWithAfTest,
       IsPasskeyFromAnotherDeviceContextMenuEnabled_NoCachedForm) {
  EXPECT_FALSE(IsPasskeyFromAnotherDeviceContextMenuEnabled(
      main_rfh(), kFormRendererId, kFieldRendererId));
}

TEST_F(ContextMenuHelperWithAfTest,
       IsPasskeyFromAnotherDeviceContextMenuEnabled_WrongFormId) {
  auto form = CreateFormWithSingleField();
  NotifyFormManagerAndWait(form);

  EXPECT_FALSE(IsPasskeyFromAnotherDeviceContextMenuEnabled(
      main_rfh(), form.renderer_id().value() + 1, kFieldRendererId));
}

TEST_F(ContextMenuHelperWithAfTest,
       IsPasskeyFromAnotherDeviceContextMenuEnabled_WrongFieldId) {
  auto form = CreateFormWithSingleField();
  NotifyFormManagerAndWait(form);

  EXPECT_FALSE(IsPasskeyFromAnotherDeviceContextMenuEnabled(
      main_rfh(), form.renderer_id().value(),
      form.fields().at(0).renderer_id().value() + 1));
}

TEST_F(ContextMenuHelperWithAfTest,
       IsPasskeyFromAnotherDeviceContextMenuEnabled_NonWebauthnField) {
  auto form = CreateFormWithSingleField();
  NotifyFormManagerAndWait(form);

  EXPECT_FALSE(IsPasskeyFromAnotherDeviceContextMenuEnabled(
      main_rfh(), form.renderer_id().value(),
      form.fields().at(0).renderer_id().value()));
}

TEST_F(
    ContextMenuHelperWithAfTest,
    IsPasskeyFromAnotherDeviceContextMenuEnabled_WebauthnFieldButNoConditionalRequest) {
  auto form = CreateFormWithSingleField(/*is_webauthn=*/true);
  NotifyFormManagerAndWait(form);

  EXPECT_FALSE(IsPasskeyFromAnotherDeviceContextMenuEnabled(
      main_rfh(), form.renderer_id().value(),
      form.fields().at(0).renderer_id().value()));
}

// TODO(crbug.com/41496601): Add more test cases after making
// ChromeWebAuthnCredentialsDelegateFactory testable.

}  // namespace
