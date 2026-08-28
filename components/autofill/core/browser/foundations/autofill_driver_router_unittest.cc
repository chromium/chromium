// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/autofill_driver_router.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {
namespace {

constexpr std::string_view kMainUrl = "https://main.frame.com/";
constexpr std::string_view kCrossOriginUrlA = "https://cross-a.frame.com/";
constexpr std::string_view kCrossOriginUrlB = "https://cross-b.frame.com/";
constexpr std::string_view kCrossOriginUrlC = "https://cross-c.frame.com/";

class FakeAutofillDriver : public TestAutofillDriver {
 public:
  enum class AutofillPermissionPolicy { kDefault, kEnabled, kDisabled };

  static std::unique_ptr<FakeAutofillDriver> CreateChildFrame(
      TestAutofillClient* client,
      const url::Origin& origin,
      FakeAutofillDriver* parent,
      AutofillPermissionPolicy autofill_policy) {
    auto driver = base::WrapUnique(new FakeAutofillDriver(client, origin));
    driver->set_autofill_manager(
        std::make_unique<TestBrowserAutofillManager>(driver.get()));
    driver->SetParent(parent);
    driver->SetLocalFrameToken(test::MakeLocalFrameToken());
    if (parent && driver->origin() != parent->origin()) {
      parent->SetRemoteFrameToken(test::MakeRemoteFrameToken(),
                                  driver->GetFrameToken());
    }
    driver->SetAutofillPermissionPolicy(autofill_policy);
    return driver;
  }

  const url::Origin& main_origin() {
    auto* driver = this;
    while (driver->GetParent()) {
      driver = driver->GetParent();
    }
    return driver->origin();
  }

  const url::Origin& origin() const { return origin_; }

  FakeAutofillDriver* GetParent() override {
    return static_cast<FakeAutofillDriver*>(TestAutofillDriver::GetParent());
  }

  FakeAutofillDriver* GetClosestSameOriginAncestor() {
    for (FakeAutofillDriver* ancestor = GetParent(); ancestor;
         ancestor = ancestor->GetParent()) {
      if (origin() == ancestor->origin()) {
        return ancestor;
      }
    }
    return nullptr;
  }

  void SetAutofillPermissionPolicy(AutofillPermissionPolicy autofill_policy) {
    FakeAutofillDriver* ancestor = GetClosestSameOriginAncestor();
    switch (autofill_policy) {
      case AutofillPermissionPolicy::kDefault:
        SetPolicyControlledFeatureAutofillEnabled(
            !GetParent() ||
            (ancestor && ancestor->IsPolicyControlledFeatureAutofillEnabled()));
        break;
      case AutofillPermissionPolicy::kEnabled:
        CHECK(!GetParent() ||
              GetParent()->IsPolicyControlledFeatureAutofillEnabled());
        SetPolicyControlledFeatureAutofillEnabled(true);
        break;
      case AutofillPermissionPolicy::kDisabled:
        SetPolicyControlledFeatureAutofillEnabled(false);
        break;
    }
  }

 private:
  explicit FakeAutofillDriver(TestAutofillClient* client,
                              const url::Origin& origin)
      : TestAutofillDriver(client), origin_(origin) {}

  const url::Origin origin_;
};

class AutofillDriverRouterTest : public testing::Test {
 protected:
  AutofillDriverRouterTest() = default;

  void TearDown() override {}

  FakeAutofillDriver& CreateDriver(
      const url::Origin& origin,
      FakeAutofillDriver* parent = nullptr,
      FakeAutofillDriver::AutofillPermissionPolicy policy =
          FakeAutofillDriver::AutofillPermissionPolicy::kDefault) {
    return static_cast<FakeAutofillDriver&>(
        client_.GetAutofillDriverFactory().TakeOwnership(
            FakeAutofillDriver::CreateChildFrame(&client_, origin, parent,
                                                 policy)));
  }

  void RegisterDriver(FakeAutofillDriver& driver) {
    FormData form;
    form.set_host_frame(driver.GetFrameToken());
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_url(GURL(driver.origin().Serialize()));

    FormFieldData field;
    field.set_host_frame(form.host_frame());
    field.set_host_form_id(form.renderer_id());
    field.set_origin(driver.origin());
    form.set_fields({field});

    router_.FormsSeen([](AutofillDriver&, std::vector<FormData>,
                         std::vector<FormGlobalId>) {},
                      driver, {form}, {});
  }

  AutofillDriverRouter& router() { return router_; }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient client_;
  AutofillDriverRouter router_;
};

url::Origin Origin(std::string_view url) {
  return url::Origin::Create(GURL(url));
}

FormFieldData CreateField(const FakeAutofillDriver& driver) {
  FormFieldData field;
  field.set_host_frame(driver.GetFrameToken());
  field.set_origin(driver.origin());
  return field;
}

// Same-origin fields are always safe to fill, regardless of sensitivity and
// policy. This corresponds to Clause (1): "The field's origin is the
// triggered origin."
TEST_F(AutofillDriverRouterTest, IsSafeToFill_SameOrigin) {
  FakeAutofillDriver& main_driver = CreateDriver(Origin(kMainUrl));
  FakeAutofillDriver& child_disabled =
      CreateDriver(Origin(kCrossOriginUrlA), &main_driver);
  FakeAutofillDriver& child_enabled =
      CreateDriver(Origin(kCrossOriginUrlB), &main_driver,
                   FakeAutofillDriver::AutofillPermissionPolicy::kEnabled);

  RegisterDriver(main_driver);
  RegisterDriver(child_disabled);
  RegisterDriver(child_enabled);

  FormFieldData field_disabled = CreateField(child_disabled);
  FormFieldData field_enabled = CreateField(child_enabled);

  // Case 1: Trigger is in an iframe with the "autofill" policy disabled.
  {
    url::Origin trigger_origin = Origin(kCrossOriginUrlA);
    url::Origin main_origin = Origin(kMainUrl);

    ASSERT_EQ(field_disabled.origin(), trigger_origin);
    ASSERT_FALSE(child_disabled.IsPolicyControlledFeatureAutofillEnabled());

    EXPECT_TRUE(router().IsSafeToFill(field_disabled, CREDIT_CARD_NUMBER,
                                      main_origin, trigger_origin));
    EXPECT_TRUE(router().IsSafeToFill(field_disabled, CREDIT_CARD_NAME_FIRST,
                                      main_origin, trigger_origin));
  }

  // Case 2: Trigger is in an iframe with the "autofill" policy enabled.
  {
    url::Origin trigger_origin = Origin(kCrossOriginUrlB);
    url::Origin main_origin = Origin(kMainUrl);

    ASSERT_EQ(field_enabled.origin(), trigger_origin);
    ASSERT_TRUE(child_enabled.IsPolicyControlledFeatureAutofillEnabled());

    EXPECT_TRUE(router().IsSafeToFill(field_enabled, CREDIT_CARD_NUMBER,
                                      main_origin, trigger_origin));
    EXPECT_TRUE(router().IsSafeToFill(field_enabled, CREDIT_CARD_NAME_FIRST,
                                      main_origin, trigger_origin));
  }
}

// Main-origin fields are safe to fill across origins from a child frame only if
// they are non-sensitive and the field's frame has the "autofill" policy
// enabled. This corresponds to Clause (2): "The field's origin is the main
// origin, the field's type is non-sensitive, and the policy-controlled
// feature "autofill" is enabled in the field's frame."
TEST_F(AutofillDriverRouterTest, IsSafeToFill_MainOriginNonSensitive) {
  FakeAutofillDriver& main_driver = CreateDriver(Origin(kMainUrl));
  FakeAutofillDriver& child_same_origin =
      CreateDriver(Origin(kMainUrl), &main_driver);
  FakeAutofillDriver& child_cross_origin_disabled =
      CreateDriver(Origin(kCrossOriginUrlC), &main_driver);
  FakeAutofillDriver& child_cross_origin_enabled =
      CreateDriver(Origin(kCrossOriginUrlB), &main_driver,
                   FakeAutofillDriver::AutofillPermissionPolicy::kEnabled);

  RegisterDriver(main_driver);
  RegisterDriver(child_same_origin);
  RegisterDriver(child_cross_origin_disabled);
  RegisterDriver(child_cross_origin_enabled);

  url::Origin trigger_origin = Origin(kCrossOriginUrlA);
  url::Origin main_origin = Origin(kMainUrl);

  FormFieldData field_main = CreateField(main_driver);
  FormFieldData field_same_origin = CreateField(child_same_origin);
  FormFieldData field_cross_disabled = CreateField(child_cross_origin_disabled);
  FormFieldData field_cross_enabled = CreateField(child_cross_origin_enabled);

  // Assert preconditions.
  ASSERT_EQ(field_main.origin(), main_origin);
  ASSERT_EQ(field_same_origin.origin(), main_origin);
  ASSERT_NE(field_cross_disabled.origin(), main_origin);
  ASSERT_NE(field_cross_disabled.origin(), trigger_origin);
  ASSERT_NE(field_cross_enabled.origin(), main_origin);
  ASSERT_NE(field_cross_enabled.origin(), trigger_origin);

  ASSERT_TRUE(main_driver.IsPolicyControlledFeatureAutofillEnabled());
  ASSERT_TRUE(child_same_origin.IsPolicyControlledFeatureAutofillEnabled());
  ASSERT_FALSE(
      child_cross_origin_disabled.IsPolicyControlledFeatureAutofillEnabled());
  ASSERT_TRUE(
      child_cross_origin_enabled.IsPolicyControlledFeatureAutofillEnabled());

  // 1. Main-origin fields (including same-origin child frames) are safe to fill
  // from a child frame only if they are non-sensitive and the frame has
  // autofill policy enabled.
  EXPECT_FALSE(router().IsSafeToFill(field_main, CREDIT_CARD_NUMBER,
                                     main_origin, trigger_origin));
  EXPECT_TRUE(router().IsSafeToFill(field_main, CREDIT_CARD_NAME_FIRST,
                                    main_origin, trigger_origin));

  EXPECT_FALSE(router().IsSafeToFill(field_same_origin, CREDIT_CARD_NUMBER,
                                     main_origin, trigger_origin));
  EXPECT_TRUE(router().IsSafeToFill(field_same_origin, CREDIT_CARD_NAME_FIRST,
                                    main_origin, trigger_origin));

  // 2. Cross-origin fields that are not the trigger origin are not safe to
  // fill, even if they are non-sensitive or have policy enabled (since trigger
  // is not main).
  EXPECT_FALSE(router().IsSafeToFill(field_cross_disabled, CREDIT_CARD_NUMBER,
                                     main_origin, trigger_origin));
  EXPECT_FALSE(router().IsSafeToFill(field_cross_disabled,
                                     CREDIT_CARD_NAME_FIRST, main_origin,
                                     trigger_origin));

  EXPECT_FALSE(router().IsSafeToFill(field_cross_enabled, CREDIT_CARD_NUMBER,
                                     main_origin, trigger_origin));
  EXPECT_FALSE(router().IsSafeToFill(field_cross_enabled,
                                     CREDIT_CARD_NAME_FIRST, main_origin,
                                     trigger_origin));
}

// Main-origin fields are not safe to fill across origins if the "autofill"
// policy is disabled on the target frame, even for non-sensitive types. This
// is a subcase of Clause (2).
TEST_F(AutofillDriverRouterTest,
       IsSafeToFill_MainOriginNonSensitive_PolicyDisabled) {
  FakeAutofillDriver& main_driver =
      CreateDriver(Origin(kMainUrl), nullptr,
                   FakeAutofillDriver::AutofillPermissionPolicy::kDisabled);
  FakeAutofillDriver& child_same_origin =
      CreateDriver(Origin(kMainUrl), &main_driver);

  RegisterDriver(main_driver);
  RegisterDriver(child_same_origin);

  url::Origin trigger_origin = Origin(kCrossOriginUrlA);
  url::Origin main_origin = Origin(kMainUrl);

  FormFieldData field_main = CreateField(main_driver);
  FormFieldData field_same_origin = CreateField(child_same_origin);

  // Assert preconditions.
  ASSERT_EQ(field_main.origin(), main_origin);
  ASSERT_EQ(field_same_origin.origin(), main_origin);
  ASSERT_FALSE(main_driver.IsPolicyControlledFeatureAutofillEnabled());
  ASSERT_FALSE(child_same_origin.IsPolicyControlledFeatureAutofillEnabled());

  // If the policy is disabled, main-origin fields are not safe to fill
  // even for non-sensitive types.
  EXPECT_FALSE(router().IsSafeToFill(field_main, CREDIT_CARD_NAME_FIRST,
                                     main_origin, trigger_origin));
  EXPECT_FALSE(router().IsSafeToFill(field_same_origin, CREDIT_CARD_NAME_FIRST,
                                     main_origin, trigger_origin));
}

// If triggered from the main origin, subframes with the "autofill" policy
// enabled are safe to fill (both sensitive and non-sensitive). This
// corresponds to Clause (3): "The triggered origin is the main origin and the
// policy-controlled feature "autofill" is enabled in the field's frame."
TEST_F(AutofillDriverRouterTest, IsSafeToFill_TriggeredFromMainOrigin) {
  FakeAutofillDriver& main_driver = CreateDriver(Origin(kMainUrl));
  FakeAutofillDriver& allowed_driver =
      CreateDriver(Origin(kCrossOriginUrlA), &main_driver,
                   FakeAutofillDriver::AutofillPermissionPolicy::kEnabled);
  FakeAutofillDriver& disallowed_driver =
      CreateDriver(Origin(kCrossOriginUrlB), &main_driver);

  RegisterDriver(main_driver);
  RegisterDriver(allowed_driver);
  RegisterDriver(disallowed_driver);

  url::Origin trigger_origin = Origin(kMainUrl);
  url::Origin main_origin = Origin(kMainUrl);

  FormFieldData field_allowed = CreateField(allowed_driver);
  FormFieldData field_disallowed = CreateField(disallowed_driver);

  // Assert preconditions.
  ASSERT_EQ(trigger_origin, main_origin);
  ASSERT_NE(field_allowed.origin(), trigger_origin);
  ASSERT_TRUE(allowed_driver.IsPolicyControlledFeatureAutofillEnabled());
  ASSERT_NE(field_disallowed.origin(), trigger_origin);
  ASSERT_FALSE(disallowed_driver.IsPolicyControlledFeatureAutofillEnabled());

  // If triggered from the main origin, subframes with the "autofill" policy
  // enabled are safe to fill (both sensitive and non-sensitive).
  EXPECT_TRUE(router().IsSafeToFill(field_allowed, CREDIT_CARD_NUMBER,
                                    main_origin, trigger_origin));
  EXPECT_TRUE(router().IsSafeToFill(field_allowed, CREDIT_CARD_NAME_FIRST,
                                    main_origin, trigger_origin));

  // Subframes with policy disabled are not safe to fill.
  EXPECT_FALSE(router().IsSafeToFill(field_disallowed, CREDIT_CARD_NUMBER,
                                     main_origin, trigger_origin));
  EXPECT_FALSE(router().IsSafeToFill(field_disallowed, CREDIT_CARD_NAME_FIRST,
                                     main_origin, trigger_origin));
}

}  // namespace
}  // namespace autofill
