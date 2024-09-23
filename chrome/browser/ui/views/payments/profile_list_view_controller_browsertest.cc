// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/label.h"

namespace payments {

autofill::AutofillProfile CreateProfileWithPartialAddress() {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile2();
  profile.SetRawInfo(autofill::ADDRESS_HOME_ADDRESS, u"");
  profile.SetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS, u"");
  profile.SetRawInfo(autofill::ADDRESS_HOME_LINE1, u"");
  profile.SetRawInfo(autofill::ADDRESS_HOME_LINE2, u"");
  profile.SetRawInfo(autofill::ADDRESS_HOME_CITY, u"");
  profile.SetRawInfo(autofill::ADDRESS_HOME_STATE, u"");
  return profile;
}

using PaymentRequestProfileListTest = PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestProfileListTest, PrioritizeCompleteness) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_free_shipping_test.html");
  autofill::AutofillProfile complete = autofill::test::GetFullProfile();
  autofill::AutofillProfile partial = CreateProfileWithPartialAddress();
  complete.FinalizeAfterImport();
  partial.FinalizeAfterImport();
  partial.set_use_count(1000);

  AddAutofillProfile(complete);
  AddAutofillProfile(partial);

  // In the Personal Data Manager, the partial address is more frecent.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  std::vector<const autofill::AutofillProfile*> profiles =
      personal_data_manager->address_data_manager().GetProfilesToSuggest();
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ(partial, *profiles[0]);
  EXPECT_EQ(complete, *profiles[1]);

  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");

  PaymentRequest* request = GetPaymentRequests().front();

  // The complete profile should be selected.
  ASSERT_TRUE(request->state()->selected_shipping_profile());
  EXPECT_EQ(complete, *request->state()->selected_shipping_profile());

  // It should appear first in the shipping profiles.
  ASSERT_EQ(2UL, request->state()->shipping_profiles().size());
  EXPECT_EQ(complete, *request->state()->shipping_profiles()[0]);
  EXPECT_EQ(partial, *request->state()->shipping_profiles()[1]);

  // And both should appear in the UI.
  OpenShippingAddressSectionScreen();
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(2u, sheet->children().size());
  const auto get_label = [sheet](size_t num) {
    constexpr int kId = static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_1);
    return static_cast<views::Label*>(sheet->children()[num]->GetViewByID(kId));
  };
  EXPECT_EQ(u"John H. Doe", get_label(0)->GetText());
  EXPECT_EQ(u"Jane A. Smith", get_label(1)->GetText());
}

}  // namespace payments
