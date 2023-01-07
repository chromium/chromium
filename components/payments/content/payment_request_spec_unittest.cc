// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_spec.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class PaymentRequestSpecTest : public testing::Test,
                               public PaymentRequestSpec::Observer {
 protected:
  ~PaymentRequestSpecTest() override = default;

  void OnSpecUpdated() override { on_spec_updated_called_ = true; }

  void RecreateSpecWithMethodData(
      std::vector<mojom::PaymentMethodDataPtr> method_data) {
    spec_ = std::make_unique<PaymentRequestSpec>(
        mojom::PaymentOptions::New(), mojom::PaymentDetails::New(),
        std::move(method_data), weak_ptr_factory_.GetWeakPtr(), "en-US");
  }

  void RecreateSpecWithOptionsAndDetails(mojom::PaymentOptionsPtr options,
                                         mojom::PaymentDetailsPtr details) {
    if (!details->total)
      details->total = mojom::PaymentItem::New();
    spec_ = std::make_unique<PaymentRequestSpec>(
        std::move(options), std::move(details),
        std::vector<mojom::PaymentMethodDataPtr>(),
        weak_ptr_factory_.GetWeakPtr(), "en-US");
  }

  PaymentRequestSpec* spec() { return spec_.get(); }

 private:
  std::unique_ptr<PaymentRequestSpec> spec_;
  bool on_spec_updated_called_ = false;
  base::WeakPtrFactory<PaymentRequestSpecTest> weak_ptr_factory_{this};
};

// Test that the last shipping option is selected, even in the case of
// updateWith.
TEST_F(PaymentRequestSpecTest, ShippingOptionsSelection) {
  std::vector<mojom::PaymentShippingOptionPtr> shipping_options;
  mojom::PaymentShippingOptionPtr option = mojom::PaymentShippingOption::New();
  option->id = "option:1";
  option->selected = false;
  shipping_options.push_back(std::move(option));
  mojom::PaymentShippingOptionPtr option2 = mojom::PaymentShippingOption::New();
  option2->id = "option:2";
  option2->selected = true;
  shipping_options.push_back(std::move(option2));
  mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
  details->shipping_options = std::move(shipping_options);

  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateSpecWithOptionsAndDetails(std::move(options), std::move(details));

  EXPECT_EQ("option:2", spec()->selected_shipping_option()->id);
  EXPECT_TRUE(spec()->selected_shipping_option_error().empty());

  // Call updateWith with option:1 now selected.
  std::vector<mojom::PaymentShippingOptionPtr> new_shipping_options;
  mojom::PaymentShippingOptionPtr new_option =
      mojom::PaymentShippingOption::New();
  new_option->id = "option:1";
  new_option->selected = true;
  new_shipping_options.push_back(std::move(new_option));
  mojom::PaymentShippingOptionPtr new_option2 =
      mojom::PaymentShippingOption::New();
  new_option2->id = "option:2";
  new_option2->selected = false;
  new_shipping_options.push_back(std::move(new_option2));
  mojom::PaymentDetailsPtr new_details = mojom::PaymentDetails::New();
  new_details->shipping_options = std::move(new_shipping_options);

  spec()->UpdateWith(std::move(new_details));

  EXPECT_EQ("option:1", spec()->selected_shipping_option()->id);
  EXPECT_TRUE(spec()->selected_shipping_option_error().empty());
}

// Test that the last shipping option is selected, even in the case of
// updateWith.
TEST_F(PaymentRequestSpecTest, ShippingOptionsSelection_NoOptionsAtAll) {
  // No options are provided at first.
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateSpecWithOptionsAndDetails(std::move(options),
                                    mojom::PaymentDetails::New());

  // No option selected, but no error either (the flow just started and no
  // address has been selected yet).
  EXPECT_EQ(nullptr, spec()->selected_shipping_option());
  EXPECT_TRUE(spec()->selected_shipping_option_error().empty());

  // Call updateWith with empty options.
  {
    mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
    details->shipping_options = std::vector<mojom::PaymentShippingOptionPtr>();
    spec()->UpdateWith(std::move(details));
  }

  // Now it's more serious. No option selected, but there is a generic error.
  EXPECT_EQ(nullptr, spec()->selected_shipping_option());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_UNSUPPORTED_SHIPPING_ADDRESS),
      spec()->selected_shipping_option_error());

  {
    // Call updateWith with still no options, but a customized error string.
    mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
    details->error = "No can do shipping.";
    spec()->UpdateWith(std::move(details));
  }

  // No option selected, but there is an error provided by the mercahnt.
  EXPECT_EQ(nullptr, spec()->selected_shipping_option());
  EXPECT_EQ(u"No can do shipping.", spec()->selected_shipping_option_error());
}

// Test that the last shipping option is selected, even in the case of
// updateWith.
TEST_F(PaymentRequestSpecTest, UpdateWithNoShippingOptions) {
  std::vector<mojom::PaymentShippingOptionPtr> shipping_options;
  mojom::PaymentShippingOptionPtr option = mojom::PaymentShippingOption::New();
  option->id = "option:1";
  option->selected = false;
  shipping_options.push_back(std::move(option));
  mojom::PaymentShippingOptionPtr option2 = mojom::PaymentShippingOption::New();
  option2->id = "option:2";
  option2->selected = true;
  shipping_options.push_back(std::move(option2));
  mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
  details->shipping_options = std::move(shipping_options);

  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateSpecWithOptionsAndDetails(std::move(options), std::move(details));

  EXPECT_EQ("option:2", spec()->selected_shipping_option()->id);
  EXPECT_TRUE(spec()->selected_shipping_option_error().empty());

  // Call updateWith with no shipping options
  spec()->UpdateWith(mojom::PaymentDetails::New());

  EXPECT_EQ("option:2", spec()->selected_shipping_option()->id);
  EXPECT_TRUE(spec()->selected_shipping_option_error().empty());
}

TEST_F(PaymentRequestSpecTest, SingleCurrencyWithoutDisplayItems) {
  mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
  mojom::PaymentItemPtr total = mojom::PaymentItem::New();
  mojom::PaymentCurrencyAmountPtr amount = mojom::PaymentCurrencyAmount::New();
  amount->currency = "USD";
  total->amount = std::move(amount);
  details->total = std::move(total);

  RecreateSpecWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                    std::move(details));
  // If the request only has a total, it must not have mixed currencies.
  EXPECT_FALSE(spec()->IsMixedCurrency());
}

TEST_F(PaymentRequestSpecTest, SingleCurrencyWithDisplayItems) {
  mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
  mojom::PaymentItemPtr total = mojom::PaymentItem::New();
  mojom::PaymentCurrencyAmountPtr amount = mojom::PaymentCurrencyAmount::New();
  amount->currency = "USD";
  total->amount = std::move(amount);
  details->total = std::move(total);
  details->display_items = std::vector<mojom::PaymentItemPtr>();

  mojom::PaymentItemPtr display_item = mojom::PaymentItem::New();
  mojom::PaymentCurrencyAmountPtr display_amount =
      mojom::PaymentCurrencyAmount::New();
  display_amount->currency = "USD";
  display_item->amount = std::move(display_amount);
  details->display_items->push_back(std::move(display_item));

  RecreateSpecWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                    std::move(details));
  // Both the total and the display item have matching currency codes, this
  // isn't a mixed currency case.
  EXPECT_FALSE(spec()->IsMixedCurrency());
}

TEST_F(PaymentRequestSpecTest, MultipleCurrenciesWithOneDisplayItem) {
  mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
  mojom::PaymentItemPtr total = mojom::PaymentItem::New();
  mojom::PaymentCurrencyAmountPtr amount = mojom::PaymentCurrencyAmount::New();
  amount->currency = "USD";
  total->amount = std::move(amount);
  details->total = std::move(total);
  details->display_items = std::vector<mojom::PaymentItemPtr>();

  mojom::PaymentItemPtr display_item = mojom::PaymentItem::New();
  mojom::PaymentCurrencyAmountPtr display_amount =
      mojom::PaymentCurrencyAmount::New();
  display_amount->currency = "CAD";
  display_item->amount = std::move(display_amount);
  details->display_items->push_back(std::move(display_item));

  RecreateSpecWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                    std::move(details));

  // The display item currency and the total's currency don't match, this is a
  // mixed currencies case.
  EXPECT_TRUE(spec()->IsMixedCurrency());
}

TEST_F(PaymentRequestSpecTest, MultipleCurrenciesWithTwoDisplayItem) {
  mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
  mojom::PaymentItemPtr total = mojom::PaymentItem::New();
  mojom::PaymentCurrencyAmountPtr amount = mojom::PaymentCurrencyAmount::New();
  amount->currency = "USD";
  total->amount = std::move(amount);
  details->total = std::move(total);
  details->display_items = std::vector<mojom::PaymentItemPtr>();

  mojom::PaymentItemPtr display_item1 = mojom::PaymentItem::New();
  mojom::PaymentCurrencyAmountPtr display_amount1 =
      mojom::PaymentCurrencyAmount::New();
  display_amount1->currency = "CAD";
  display_item1->amount = std::move(display_amount1);
  details->display_items->push_back(std::move(display_item1));

  mojom::PaymentItemPtr display_item2 = mojom::PaymentItem::New();
  mojom::PaymentCurrencyAmountPtr display_amount2 =
      mojom::PaymentCurrencyAmount::New();
  display_amount2->currency = "USD";
  display_item2->amount = std::move(display_amount2);
  details->display_items->push_back(std::move(display_item2));

  RecreateSpecWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                    std::move(details));

  // At least one of the display items has a different currency, this is a mixed
  // currency case.
  EXPECT_TRUE(spec()->IsMixedCurrency());
}

TEST_F(PaymentRequestSpecTest, RetryWithShippingAddressErrors) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateSpecWithOptionsAndDetails(std::move(options),
                                    mojom::PaymentDetails::New());

  EXPECT_FALSE(spec()->has_shipping_address_error());

  mojom::AddressErrorsPtr shipping_address_errors = mojom::AddressErrors::New();
  shipping_address_errors->address_line = "Invalid address line";
  shipping_address_errors->city = "Invalid city";

  mojom::PaymentValidationErrorsPtr errors =
      mojom::PaymentValidationErrors::New();
  errors->shipping_address = std::move(shipping_address_errors);

  spec()->Retry(std::move(errors));

  EXPECT_EQ(u"Invalid city",
            spec()->GetShippingAddressError(autofill::ADDRESS_HOME_CITY));
  EXPECT_EQ(
      u"Invalid address line",
      spec()->GetShippingAddressError(autofill::ADDRESS_HOME_STREET_ADDRESS));

  EXPECT_TRUE(spec()->has_shipping_address_error());
}

TEST_F(PaymentRequestSpecTest, RetryWithPayerErrors) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_email = true;
  options->request_payer_name = true;
  options->request_payer_phone = true;
  RecreateSpecWithOptionsAndDetails(std::move(options),
                                    mojom::PaymentDetails::New());

  EXPECT_FALSE(spec()->has_payer_error());

  mojom::PayerErrorsPtr payer_errors = mojom::PayerErrors::New();
  payer_errors->email = "Invalid email";
  payer_errors->name = "Invalid name";
  payer_errors->phone = "Invalid phone";

  mojom::PaymentValidationErrorsPtr errors =
      mojom::PaymentValidationErrors::New();
  errors->payer = std::move(payer_errors);

  spec()->Retry(std::move(errors));

  EXPECT_EQ(u"Invalid email", spec()->GetPayerError(autofill::EMAIL_ADDRESS));
  EXPECT_EQ(u"Invalid name", spec()->GetPayerError(autofill::NAME_FULL));
  EXPECT_EQ(u"Invalid phone",
            spec()->GetPayerError(autofill::PHONE_HOME_WHOLE_NUMBER));

  EXPECT_TRUE(spec()->has_payer_error());
}
}  // namespace payments
