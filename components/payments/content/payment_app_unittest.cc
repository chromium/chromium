// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_app.h"

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/content/autofill_payment_app.h"
#include "components/payments/content/service_worker_payment_app.h"
#include "components/payments/core/features.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/browser/supported_delegations.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

namespace {

enum class RequiredPaymentOptions {
  // None of the shipping address or contact information is required.
  kNone,
  // Shipping Address is required.
  kShippingAddress,
  // Payer's contact information(phone, name, email) is required.
  kContactInformation,
  // Payer's email is required.
  kPayerEmail,
  // Both contact information and shipping address are required.
  kContactInformationAndShippingAddress,
};

}  // namespace

class PaymentAppTest : public testing::TestWithParam<RequiredPaymentOptions>,
                       public PaymentRequestSpec::Observer {
 protected:
  PaymentAppTest()
      : address_(autofill::test::GetFullProfile()),
        local_card_(autofill::test::GetCreditCard()),
        billing_profiles_({&address_}),
        required_options_(GetParam()) {
    local_card_.set_billing_address_id(address_.guid());
    CreateSpec();
    web_contents_ =
        test_web_contents_factory_.CreateWebContents(&browser_context_);
  }

  std::unique_ptr<ServiceWorkerPaymentApp> CreateServiceWorkerPaymentApp(
      bool can_preselect,
      bool handles_shipping,
      bool handles_name,
      bool handles_phone,
      bool handles_email) {
    std::unique_ptr<content::StoredPaymentApp> stored_app =
        std::make_unique<content::StoredPaymentApp>();
    stored_app->registration_id = 123456;
    stored_app->scope = GURL("https://bobpay.com");
    stored_app->name = "bobpay";
    stored_app->icon = std::make_unique<SkBitmap>();
    if (can_preselect) {
      PopulateIcon(stored_app->icon.get());
    }
    if (handles_shipping) {
      stored_app->supported_delegations.shipping_address = true;
    }
    if (handles_name) {
      stored_app->supported_delegations.payer_name = true;
    }
    if (handles_phone) {
      stored_app->supported_delegations.payer_phone = true;
    }
    if (handles_email) {
      stored_app->supported_delegations.payer_email = true;
    }

    return std::make_unique<ServiceWorkerPaymentApp>(
        web_contents_, GURL("https://testmerchant.com"),
        GURL("https://testmerchant.com/bobpay"), spec_->AsWeakPtr(),
        std::move(stored_app), /*is_incognito=*/false,
        /*show_processing_spinner=*/base::DoNothing());
  }

  std::unique_ptr<ServiceWorkerPaymentApp>
  CreateInstallableServiceWorkerPaymentApp(bool can_preselect,
                                           bool handles_shipping,
                                           bool handles_name,
                                           bool handles_phone,
                                           bool handles_email) {
    auto installable_app = std::make_unique<WebAppInstallationInfo>();
    installable_app->name = "installable_pay";
    installable_app->sw_js_url = "https://pay.example/app.js";
    installable_app->sw_scope = "https://pay.example";
    installable_app->icon = std::make_unique<SkBitmap>();
    if (can_preselect)
      PopulateIcon(installable_app->icon.get());
    if (handles_shipping)
      installable_app->supported_delegations.shipping_address = true;
    if (handles_name)
      installable_app->supported_delegations.payer_name = true;
    if (handles_phone)
      installable_app->supported_delegations.payer_phone = true;
    if (handles_email)
      installable_app->supported_delegations.payer_email = true;

    return std::make_unique<ServiceWorkerPaymentApp>(
        web_contents_, GURL("https://merchant.example"),
        GURL("https://merchant.example/iframe"), spec_->AsWeakPtr(),
        std::move(installable_app), "https://pay.example",
        /*is_incognito=*/false, /*show_processing_spinner=*/base::DoNothing());
  }

  static void PopulateIcon(SkBitmap* icon) {
    constexpr int kBitmapDimension = 16;
    icon->allocN32Pixels(kBitmapDimension, kBitmapDimension);
    icon->eraseColor(SK_ColorRED);
  }

  autofill::CreditCard& local_credit_card() { return local_card_; }
  std::vector<autofill::AutofillProfile*>& billing_profiles() {
    return billing_profiles_;
  }

  RequiredPaymentOptions required_options() const { return required_options_; }

 private:
  // PaymentRequestSpec::Observer
  void OnSpecUpdated() override {}

  void CreateSpec() {
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    mojom::PaymentOptionsPtr payment_options = mojom::PaymentOptions::New();
    switch (required_options_) {
      case RequiredPaymentOptions::kNone:
        break;
      case RequiredPaymentOptions::kShippingAddress:
        payment_options->request_shipping = true;
        break;
      case RequiredPaymentOptions::kContactInformation:
        payment_options->request_payer_name = true;
        payment_options->request_payer_email = true;
        payment_options->request_payer_phone = true;
        break;
      case RequiredPaymentOptions::kPayerEmail:
        payment_options->request_payer_email = true;
        break;
      case RequiredPaymentOptions::kContactInformationAndShippingAddress:
        payment_options->request_shipping = true;
        payment_options->request_payer_name = true;
        payment_options->request_payer_email = true;
        payment_options->request_payer_phone = true;
        break;
    }
    spec_ = std::make_unique<PaymentRequestSpec>(
        std::move(payment_options), mojom::PaymentDetails::New(),
        std::move(method_data), weak_ptr_factory_.GetWeakPtr(), "en-US");
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  content::TestWebContentsFactory test_web_contents_factory_;
  content::WebContents* web_contents_;
  autofill::AutofillProfile address_;
  autofill::CreditCard local_card_;
  std::vector<autofill::AutofillProfile*> billing_profiles_;
  RequiredPaymentOptions required_options_;
  std::unique_ptr<PaymentRequestSpec> spec_;
  base::WeakPtrFactory<PaymentAppTest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentAppTest);
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PaymentAppTest,
    ::testing::Values(
        RequiredPaymentOptions::kNone,
        RequiredPaymentOptions::kShippingAddress,
        RequiredPaymentOptions::kContactInformation,
        RequiredPaymentOptions::kPayerEmail,
        RequiredPaymentOptions::kContactInformationAndShippingAddress));

TEST_P(PaymentAppTest, SortApps) {
  std::vector<PaymentApp*> apps;
  // Add a card with no billing address.
  autofill::CreditCard card_with_no_address = local_credit_card();
  card_with_no_address.set_billing_address_id("");
  AutofillPaymentApp cc_app_with_no_address(
      "visa", card_with_no_address, billing_profiles(), "en-US", nullptr);
  apps.push_back(&cc_app_with_no_address);

  // Add an expired card.
  autofill::CreditCard expired_card = local_credit_card();
  expired_card.SetExpirationYear(2016);
  AutofillPaymentApp expired_cc_app("visa", expired_card, billing_profiles(),
                                    "en-US", nullptr);
  apps.push_back(&expired_cc_app);

  // Add a non-preselectable sw based payment app.
  std::unique_ptr<ServiceWorkerPaymentApp> non_preselectable_sw_app =
      CreateServiceWorkerPaymentApp(
          false /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  apps.push_back(non_preselectable_sw_app.get());

  // Add a preselectable sw based payment app.
  std::unique_ptr<ServiceWorkerPaymentApp> preselectable_sw_app =
      CreateServiceWorkerPaymentApp(
          true /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  apps.push_back(preselectable_sw_app.get());

  // Add a card with no name.
  autofill::CreditCard card_with_no_name = local_credit_card();
  card_with_no_name.SetInfo(
      autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL),
      base::ASCIIToUTF16(""), "en-US");
  AutofillPaymentApp cc_app_with_no_name("visa", card_with_no_name,
                                         billing_profiles(), "en-US", nullptr);
  apps.push_back(&cc_app_with_no_name);

  // Add a complete card.
  autofill::CreditCard complete_card = local_credit_card();
  AutofillPaymentApp complete_cc_app("visa", complete_card, billing_profiles(),
                                     "en-US", nullptr);
  apps.push_back(&complete_cc_app);

  // Add a card with no number.
  autofill::CreditCard card_with_no_number = local_credit_card();
  card_with_no_number.SetNumber(base::ASCIIToUTF16(""));
  AutofillPaymentApp cc_app_with_no_number(
      "visa", card_with_no_number, billing_profiles(), "en-US", nullptr);
  apps.push_back(&cc_app_with_no_number);

  // Add a complete matching card that is most frequently used.
  autofill::CreditCard complete_frequently_used_card = local_credit_card();
  AutofillPaymentApp complete_frequently_used_cc_app(
      "visa", complete_frequently_used_card, billing_profiles(), "en-US",
      nullptr);
  apps.push_back(&complete_frequently_used_cc_app);
  // Record use of this card.
  complete_frequently_used_cc_app.credit_card()->RecordAndLogUse();

  // Sort the apps and validate the new order.
  PaymentApp::SortApps(&apps);
  size_t i = 0;
  EXPECT_EQ(apps[i++], preselectable_sw_app.get());
  EXPECT_EQ(apps[i++], non_preselectable_sw_app.get());

  // Autfill apps (credit cards) come after sw apps.
  EXPECT_EQ(apps[i++], &complete_frequently_used_cc_app);
  EXPECT_EQ(apps[i++], &complete_cc_app);
  EXPECT_EQ(apps[i++], &expired_cc_app);
  EXPECT_EQ(apps[i++], &cc_app_with_no_name);
  EXPECT_EQ(apps[i++], &cc_app_with_no_address);
  EXPECT_EQ(apps[i++], &cc_app_with_no_number);
}

TEST_P(PaymentAppTest, SortAppsBasedOnSupportedDelegations) {
  std::vector<PaymentApp*> apps;
  // Add a preselectable sw based payment app which does not support
  // shipping or contact delegation.
  std::unique_ptr<ServiceWorkerPaymentApp> does_not_support_delegations =
      CreateServiceWorkerPaymentApp(
          true /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  apps.push_back(does_not_support_delegations.get());

  // Add a preselectable sw based payment app which handles shipping.
  std::unique_ptr<ServiceWorkerPaymentApp> handles_shipping_address =
      CreateServiceWorkerPaymentApp(
          true /* = can_preselect */, true /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  apps.push_back(handles_shipping_address.get());

  // Add a preselectable sw based payment app which handles payer's
  // email.
  std::unique_ptr<ServiceWorkerPaymentApp> handles_payer_email =
      CreateServiceWorkerPaymentApp(
          true /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          true /* = handles_email */);
  apps.push_back(handles_payer_email.get());

  // Add a preselectable sw based payment app which handles contact
  // information.
  std::unique_ptr<ServiceWorkerPaymentApp> handles_contact_info =
      CreateServiceWorkerPaymentApp(
          true /* = can_preselect */, false /* = handles_shipping */,
          true /* = handles_name */, true /* = handles_phone */,
          true /* = handles_email */);
  apps.push_back(handles_contact_info.get());

  // Add a preselectable sw based payment app which handles both shipping
  // address and contact information.
  std::unique_ptr<ServiceWorkerPaymentApp> handles_shipping_and_contact =
      CreateServiceWorkerPaymentApp(
          true /* = can_preselect */, true /* = handles_shipping */,
          true /* = handles_name */, true /* = handles_phone */,
          true /* = handles_email */);
  apps.push_back(handles_shipping_and_contact.get());

  PaymentApp::SortApps(&apps);
  size_t i = 0;

  switch (required_options()) {
    case RequiredPaymentOptions::kNone: {
      // When no payemnt option is required the order of the payment apps
      // does not change.
      EXPECT_EQ(apps[i++], does_not_support_delegations.get());
      EXPECT_EQ(apps[i++], handles_shipping_address.get());
      EXPECT_EQ(apps[i++], handles_payer_email.get());
      EXPECT_EQ(apps[i++], handles_contact_info.get());
      EXPECT_EQ(apps[i++], handles_shipping_and_contact.get());
      break;
    }
    case RequiredPaymentOptions::kShippingAddress: {
      // apps that handle shipping address come first.
      EXPECT_EQ(apps[i++], handles_shipping_address.get());
      EXPECT_EQ(apps[i++], handles_shipping_and_contact.get());
      // The order is unchanged for apps that do not handle shipping.
      EXPECT_EQ(apps[i++], does_not_support_delegations.get());
      EXPECT_EQ(apps[i++], handles_payer_email.get());
      EXPECT_EQ(apps[i++], handles_contact_info.get());
      break;
    }
    case RequiredPaymentOptions::kContactInformation: {
      // apps that handle all required contact information come first.
      EXPECT_EQ(apps[i++], handles_contact_info.get());
      EXPECT_EQ(apps[i++], handles_shipping_and_contact.get());
      // The app that partially handles contact information comes next.
      EXPECT_EQ(apps[i++], handles_payer_email.get());
      // The order for apps that do not handle contact information is not
      // changed.
      EXPECT_EQ(apps[i++], does_not_support_delegations.get());
      EXPECT_EQ(apps[i++], handles_shipping_address.get());
      break;
    }
    case RequiredPaymentOptions::kPayerEmail: {
      EXPECT_EQ(apps[i++], handles_payer_email.get());
      EXPECT_EQ(apps[i++], handles_contact_info.get());
      EXPECT_EQ(apps[i++], handles_shipping_and_contact.get());
      EXPECT_EQ(apps[i++], does_not_support_delegations.get());
      EXPECT_EQ(apps[i++], handles_shipping_address.get());
      break;
    }
    case RequiredPaymentOptions::kContactInformationAndShippingAddress: {
      EXPECT_EQ(apps[i++], handles_shipping_and_contact.get());
      EXPECT_EQ(apps[i++], handles_shipping_address.get());
      EXPECT_EQ(apps[i++], handles_contact_info.get());
      EXPECT_EQ(apps[i++], handles_payer_email.get());
      EXPECT_EQ(apps[i++], does_not_support_delegations.get());
      break;
    }
  }
}

TEST_P(PaymentAppTest, SortApps_DownRankJustInTimePaymentApp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDownRankJustInTimePaymentApp);

  std::vector<PaymentApp*> apps;

  // Add a card with no billing address.
  autofill::CreditCard card_with_no_address = local_credit_card();
  card_with_no_address.set_billing_address_id("");
  AutofillPaymentApp cc_app_with_no_address(
      "visa", card_with_no_address, billing_profiles(), "en-US", nullptr);
  apps.push_back(&cc_app_with_no_address);

  // Add an expired card.
  autofill::CreditCard expired_card = local_credit_card();
  expired_card.SetExpirationYear(2016);
  AutofillPaymentApp expired_cc_app("visa", expired_card, billing_profiles(),
                                    "en-US", nullptr);
  apps.push_back(&expired_cc_app);

  // Add a card with no number.
  autofill::CreditCard card_with_no_number = local_credit_card();
  card_with_no_number.SetNumber(base::ASCIIToUTF16(""));
  AutofillPaymentApp cc_app_with_no_number(
      "visa", card_with_no_number, billing_profiles(), "en-US", nullptr);
  apps.push_back(&cc_app_with_no_number);

  // Add a card with no name.
  autofill::CreditCard card_with_no_name = local_credit_card();
  card_with_no_name.SetInfo(
      autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL),
      base::ASCIIToUTF16(""), "en-US");
  AutofillPaymentApp cc_app_with_no_name("visa", card_with_no_name,
                                         billing_profiles(), "en-US", nullptr);
  apps.push_back(&cc_app_with_no_name);

  // Add a just-in-time installable sw based payment app.
  std::unique_ptr<ServiceWorkerPaymentApp> installable_sw_app =
      CreateInstallableServiceWorkerPaymentApp(
          true /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  apps.push_back(installable_sw_app.get());

  // Add an installed, non-preselectable sw based payment app.
  std::unique_ptr<ServiceWorkerPaymentApp> non_preselectable_sw_app =
      CreateServiceWorkerPaymentApp(
          false /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  apps.push_back(non_preselectable_sw_app.get());

  // Add an installed, preselectable, sw based payment app.
  std::unique_ptr<ServiceWorkerPaymentApp> preselectable_sw_app =
      CreateServiceWorkerPaymentApp(
          true /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  apps.push_back(preselectable_sw_app.get());

  // Add a complete card.
  autofill::CreditCard complete_card = local_credit_card();
  AutofillPaymentApp complete_cc_app("visa", complete_card, billing_profiles(),
                                     "en-US", nullptr);
  apps.push_back(&complete_cc_app);

  // Add a complete matching card that is most frequently used.
  autofill::CreditCard complete_frequently_used_card = local_credit_card();
  AutofillPaymentApp complete_frequently_used_cc_app(
      "visa", complete_frequently_used_card, billing_profiles(), "en-US",
      nullptr);
  apps.push_back(&complete_frequently_used_cc_app);
  // Record use of this card.
  complete_frequently_used_cc_app.credit_card()->RecordAndLogUse();

  // Sort the apps and validate the new order.
  PaymentApp::SortApps(&apps);
  size_t i = 0;

  // Installed sw based payment handlers come first.
  EXPECT_EQ(apps[i++], preselectable_sw_app.get());
  EXPECT_EQ(apps[i++], non_preselectable_sw_app.get());
  // Complete autofill apps are sorted by frecency.
  EXPECT_EQ(apps[i++], &complete_frequently_used_cc_app);
  EXPECT_EQ(apps[i++], &complete_cc_app);
  EXPECT_EQ(apps[i++], &expired_cc_app);
  // Just-in-time installable sw based payment apps come after autofill apps.
  EXPECT_EQ(apps[i++], installable_sw_app.get());
  // Incomplete autofill apps (credit cards) come last.
  EXPECT_EQ(apps[i++], &cc_app_with_no_name);
  EXPECT_EQ(apps[i++], &cc_app_with_no_address);
  EXPECT_EQ(apps[i++], &cc_app_with_no_number);
}

}  // namespace payments
