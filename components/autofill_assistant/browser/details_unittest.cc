// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/details.h"

#include "base/test/icu_test_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

using ::testing::Eq;

void SetDateTimeProto(DateTimeProto* proto,
                      int year,
                      int month,
                      int day,
                      int hour,
                      int minute,
                      int second) {
  auto* date = proto->mutable_date();
  date->set_year(year);
  date->set_month(month);
  date->set_day(day);
  auto* time = proto->mutable_time();
  time->set_hour(hour);
  time->set_minute(minute);
  time->set_second(second);
}

class DetailsTest : public testing::Test {
 public:
  DetailsTest() {}

  void SetUp() override { autofill::CountryNames::SetLocaleString("us-en"); }

 protected:
  std::unique_ptr<autofill::CreditCard> MakeCreditCard() {
    return std::make_unique<autofill::CreditCard>();
  }

  std::unique_ptr<autofill::AutofillProfile> MakeAutofillProfile() {
    // The email contains a UTF-8 smiley face.
    auto profile = std::make_unique<autofill::AutofillProfile>();
    autofill::test::SetProfileInfo(profile.get(), "Charles", "Hardin", "Holley",
                                   "\xE2\x98\xBA@gmail.com", "Decca",
                                   "123 Apple St.", "unit 6", "Lubbock",
                                   "Texas", "79401", "US", "23456789012");
    return profile;
  }

  UserData user_data_;
  CollectUserDataOptions user_data_options_;
};

TEST_F(DetailsTest, UpdateFromParametersEmpty) {
  Details details;
  // Nothing has to be updated.
  auto context = TriggerContext::CreateEmpty();
  EXPECT_FALSE(details.UpdateFromParameters(*context));
}

TEST_F(DetailsTest, UpdateFromParametersShowInitialNoUpdate) {
  std::map<std::string, std::string> parameters;
  parameters["DETAILS_SHOW_INITIAL"] = "false";
  auto context = TriggerContext::Create(parameters, "exps");

  Details details;
  EXPECT_FALSE(details.UpdateFromParameters(*context));
}

TEST_F(DetailsTest, UpdateFromParametersSetsPlaceholderFlags) {
  std::map<std::string, std::string> parameters;
  parameters["DETAILS_SHOW_INITIAL"] = "true";

  auto context = TriggerContext::Create(parameters, "exps");

  Details details;
  details.UpdateFromParameters(*context);

  EXPECT_TRUE(details.animatePlaceholders());
  EXPECT_TRUE(details.showImagePlaceholder());
}

TEST_F(DetailsTest, UpdateFromParametersUpdateFromDetails) {
  std::map<std::string, std::string> parameters;
  parameters["DETAILS_SHOW_INITIAL"] = "true";
  parameters["DETAILS_TITLE"] = "title";
  parameters["DETAILS_DESCRIPTION_LINE_1"] = "line1";
  parameters["DETAILS_DESCRIPTION_LINE_2"] = "line2";
  parameters["DETAILS_DESCRIPTION_LINE_3"] = "Est. total";
  parameters["DETAILS_IMAGE_URL"] = "image";
  parameters["DETAILS_IMAGE_ACCESSIBILITY_HINT"] = "hint";
  parameters["DETAILS_IMAGE_CLICKTHROUGH_URL"] = "clickthrough";
  parameters["DETAILS_TOTAL_PRICE_LABEL"] = "total";
  parameters["DETAILS_TOTAL_PRICE"] = "12";

  auto context = TriggerContext::Create(parameters, "exps");

  Details details;
  EXPECT_TRUE(details.UpdateFromParameters(*context));

  EXPECT_TRUE(details.animatePlaceholders());
  EXPECT_THAT(details.title(), Eq("title"));
  EXPECT_THAT(details.descriptionLine1(), Eq("line1"));
  EXPECT_THAT(details.descriptionLine2(), Eq("line2"));
  EXPECT_THAT(details.priceAttribution(), Eq("Est. total"));
  EXPECT_THAT(details.imageUrl(),
              Eq("image"));  // Overwrites show_image_placeholder
  EXPECT_THAT(details.imageAccessibilityHint(), Eq("hint"));
  EXPECT_TRUE(details.imageAllowClickthrough());
  EXPECT_THAT(details.imageClickthroughUrl(), Eq("clickthrough"));
  EXPECT_THAT(details.totalPriceLabel(), Eq("total"));
  EXPECT_THAT(details.totalPrice(), Eq("12"));
}

TEST_F(DetailsTest, UpdateFromParametersBackwardsCompatibility) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_US");

  std::map<std::string, std::string> parameters;
  parameters["MOVIES_MOVIE_NAME"] = "movie_name";
  parameters["MOVIES_THEATER_NAME"] = "movie_theater";
  parameters["MOVIES_SCREENING_DATETIME"] = "2019-09-26T16:40:02";

  auto context = TriggerContext::Create(parameters, "exps");

  Details details;
  EXPECT_TRUE(details.UpdateFromParameters(*context));

  EXPECT_TRUE(details.animatePlaceholders());
  EXPECT_TRUE(details.showImagePlaceholder());
  EXPECT_THAT(details.title(), Eq("movie_name"));
  EXPECT_THAT(details.descriptionLine2(), Eq("movie_theater"));
  EXPECT_THAT(details.descriptionLine1(),
              Eq("4:40 PM \xE2\x80\xA2 Thu, Sep 26"));
}

TEST_F(DetailsTest, UpdateFromProtoNoDetails) {
  Details details;
  EXPECT_FALSE(Details::UpdateFromProto(ShowDetailsProto(), &details));
}

TEST_F(DetailsTest, UpdateFromProtoBackwardsCompatibility) {
  ShowDetailsProto proto;
  proto.mutable_details()->set_title("title");
  proto.mutable_details()->set_description("description");

  Details details;
  EXPECT_TRUE(Details::UpdateFromProto(proto, &details));

  EXPECT_THAT(details.title(), Eq("title"));
  EXPECT_THAT(details.descriptionLine2(), Eq("description"));
}

TEST_F(DetailsTest, UpdateFromContactDetailsNoAddressInMemory) {
  EXPECT_FALSE(Details::UpdateFromContactDetails(
      ShowDetailsProto(), &user_data_, &user_data_options_, nullptr));
}

TEST_F(DetailsTest, UpdateFromContactDetailsNoUserDataOptions) {
  EXPECT_FALSE(Details::UpdateFromContactDetails(
      ShowDetailsProto(), &user_data_, /* user_data_options = */ nullptr,
      nullptr));
}

TEST_F(DetailsTest, UpdateFromContactDetailsNoContactInfoRequested) {
  ShowDetailsProto proto;
  proto.set_contact_details("contact");
  user_data_.selected_addresses_["contact"] = MakeAutofillProfile();
  user_data_options_.request_payer_name = false;
  user_data_options_.request_payer_email = false;
  EXPECT_FALSE(Details::UpdateFromContactDetails(proto, &user_data_,
                                                 &user_data_options_, nullptr));
}

TEST_F(DetailsTest, UpdateFromContactDetails) {
  ShowDetailsProto proto;
  proto.set_contact_details("contact");
  user_data_.selected_addresses_["contact"] = MakeAutofillProfile();
  user_data_options_.request_payer_name = true;
  user_data_options_.request_payer_email = true;

  Details details;
  EXPECT_TRUE(Details::UpdateFromContactDetails(proto, &user_data_,
                                                &user_data_options_, &details));

  EXPECT_THAT(details.title(),
              Eq(l10n_util::GetStringUTF8(IDS_PAYMENTS_CONTACT_DETAILS_LABEL)));
  EXPECT_THAT(details.descriptionLine1(), Eq("Charles Hardin Holley"));
  EXPECT_THAT(details.descriptionLine2(), Eq("\xE2\x98\xBA@gmail.com"));
}

TEST_F(DetailsTest, UpdateFromContactOnlyName) {
  ShowDetailsProto proto;
  proto.set_contact_details("contact");
  user_data_.selected_addresses_["contact"] = MakeAutofillProfile();
  user_data_options_.request_payer_name = true;
  user_data_options_.request_payer_email = false;

  Details details;
  EXPECT_TRUE(Details::UpdateFromContactDetails(proto, &user_data_,
                                                &user_data_options_, &details));

  EXPECT_THAT(details.title(),
              Eq(l10n_util::GetStringUTF8(IDS_PAYMENTS_CONTACT_DETAILS_LABEL)));
  EXPECT_THAT(details.descriptionLine1(), Eq("Charles Hardin Holley"));
  EXPECT_THAT(details.descriptionLine2(), Eq(""));
}

TEST_F(DetailsTest, UpdateFromContactOnlyEmail) {
  ShowDetailsProto proto;
  proto.set_contact_details("contact");
  user_data_.selected_addresses_["contact"] = MakeAutofillProfile();
  user_data_options_.request_payer_name = false;
  user_data_options_.request_payer_email = true;

  Details details;
  EXPECT_TRUE(Details::UpdateFromContactDetails(proto, &user_data_,
                                                &user_data_options_, &details));

  EXPECT_THAT(details.title(),
              Eq(l10n_util::GetStringUTF8(IDS_PAYMENTS_CONTACT_DETAILS_LABEL)));
  EXPECT_THAT(details.descriptionLine1(), Eq(""));
  EXPECT_THAT(details.descriptionLine2(), Eq("\xE2\x98\xBA@gmail.com"));
}

TEST_F(DetailsTest, UpdateFromShippingAddressNoAddressInMemory) {
  EXPECT_FALSE(Details::UpdateFromShippingAddress(ShowDetailsProto(),
                                                  &user_data_, nullptr));
}

TEST_F(DetailsTest, UpdateFromShippingAddress) {
  ShowDetailsProto proto;
  proto.set_shipping_address("shipping");
  user_data_.selected_addresses_["shipping"] = MakeAutofillProfile();

  Details details;
  EXPECT_TRUE(Details::UpdateFromShippingAddress(proto, &user_data_, &details));

  EXPECT_THAT(
      details.title(),
      Eq(l10n_util::GetStringUTF8(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL)));
  EXPECT_THAT(details.descriptionLine1(), Eq("Charles Hardin Holley"));
  EXPECT_THAT(details.descriptionLine2(),
              Eq("123 Apple St.\nunit 6 79401 Lubbock US"));
}

TEST_F(DetailsTest, UpdateFromSelectedCreditCardEmptyMemory) {
  ShowDetailsProto proto;
  proto.set_credit_card(true);
  EXPECT_FALSE(Details::UpdateFromSelectedCreditCard(ShowDetailsProto(),
                                                     &user_data_, nullptr));
}

TEST_F(DetailsTest, UpdateFromSelectedCreditCardNotRequested) {
  ShowDetailsProto proto;
  proto.set_credit_card(false);
  user_data_.selected_card_ = MakeCreditCard();
  EXPECT_FALSE(Details::UpdateFromSelectedCreditCard(ShowDetailsProto(),
                                                     &user_data_, nullptr));
}

TEST_F(DetailsTest, UpdateFromCreditCard) {
  ShowDetailsProto proto;
  proto.set_credit_card(true);
  user_data_.selected_card_ = MakeCreditCard();

  Details details;
  EXPECT_TRUE(
      Details::UpdateFromSelectedCreditCard(proto, &user_data_, &details));

  EXPECT_THAT(
      details.title(),
      Eq(l10n_util::GetStringUTF8(IDS_PAYMENTS_METHOD_OF_PAYMENT_LABEL)));
  // The credit card string contains 4 non-ascii dots, we just check that it
  // does contain something.
  EXPECT_FALSE(details.descriptionLine1().empty());
}

TEST_F(DetailsTest, GetTitleMaxLines) {
  Details details;

  ShowDetailsProto proto_no_description;
  proto_no_description.mutable_details()->set_title("title");
  EXPECT_TRUE(Details::UpdateFromProto(proto_no_description, &details));
  EXPECT_THAT(details.titleMaxLines(), Eq(3));

  ShowDetailsProto proto_description1;
  proto_description1.mutable_details()->set_title("title");
  proto_description1.mutable_details()->set_description_line_1("line 1");
  EXPECT_TRUE(Details::UpdateFromProto(proto_description1, &details));
  EXPECT_THAT(details.titleMaxLines(), Eq(2));

  ShowDetailsProto proto_description2;
  proto_description2.mutable_details()->set_title("title");
  proto_description2.mutable_details()->set_description_line_2("line 2");
  EXPECT_TRUE(Details::UpdateFromProto(proto_description2, &details));
  EXPECT_THAT(details.titleMaxLines(), Eq(2));

  ShowDetailsProto proto_description1_date;
  proto_description1_date.mutable_details()->set_title("title");
  SetDateTimeProto(
      proto_description1_date.mutable_details()->mutable_datetime(), 2019, 9,
      26, 16, 40, 2);
  EXPECT_TRUE(Details::UpdateFromProto(proto_description1_date, &details));
  EXPECT_THAT(details.titleMaxLines(), Eq(2));

  ShowDetailsProto proto_both_descriptions;
  proto_both_descriptions.mutable_details()->set_title("title");
  proto_both_descriptions.mutable_details()->set_description_line_1("line 1");
  proto_both_descriptions.mutable_details()->set_description_line_2("line 2");
  EXPECT_TRUE(Details::UpdateFromProto(proto_both_descriptions, &details));
  EXPECT_THAT(details.titleMaxLines(), Eq(1));
}

TEST_F(DetailsTest, GetDescriptionLine1) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;

  Details details;

  ShowDetailsProto proto_description;
  proto_description.mutable_details()->set_description_line_1("line 1");
  EXPECT_TRUE(Details::UpdateFromProto(proto_description, &details));
  EXPECT_THAT(details.descriptionLine1(), Eq("line 1"));

  base::i18n::SetICUDefaultLocale("en_US");
  ShowDetailsProto proto_date;
  SetDateTimeProto(proto_date.mutable_details()->mutable_datetime(), 2019, 9,
                   25, 15, 16, 0);
  EXPECT_TRUE(Details::UpdateFromProto(proto_date, &details));
  EXPECT_THAT(details.descriptionLine1(),
              Eq("3:16 PM \xE2\x80\xA2 Wed, Sep 25"));

  base::i18n::SetICUDefaultLocale("de_DE");
  ShowDetailsProto proto_date_de;
  SetDateTimeProto(proto_date.mutable_details()->mutable_datetime(), 2019, 9,
                   25, 15, 16, 0);
  EXPECT_TRUE(Details::UpdateFromProto(proto_date, &details));
  EXPECT_THAT(details.descriptionLine1(),
              Eq("3:16 PM \xE2\x80\xA2 Mi., 25. Sept."));

  ShowDetailsProto proto_empty;
  proto_empty.mutable_details()->set_title("title");
  EXPECT_TRUE(Details::UpdateFromProto(proto_empty, &details));
  EXPECT_THAT(details.descriptionLine1(), Eq(""));
}

TEST_F(DetailsTest, GetDescriptionLine2) {
  Details details;

  ShowDetailsProto proto_description;
  proto_description.mutable_details()->set_title("title");
  proto_description.mutable_details()->set_description_line_2("line 2");
  EXPECT_TRUE(Details::UpdateFromProto(proto_description, &details));
  EXPECT_THAT(details.descriptionLine2(), Eq("line 2"));
}

TEST_F(DetailsTest, GetDescriptionLine3) {
  Details details;

  ShowDetailsProto proto_no_price;
  proto_no_price.mutable_details()->set_title("title");
  proto_no_price.mutable_details()->set_description_line_3("line 3");
  EXPECT_TRUE(Details::UpdateFromProto(proto_no_price, &details));
  EXPECT_THAT(details.descriptionLine3(), Eq("line 3"));

  ShowDetailsProto proto_with_price;
  proto_with_price.mutable_details()->set_title("title");
  proto_with_price.mutable_details()->set_description_line_3("Est. total");
  proto_with_price.mutable_details()->set_total_price("$2.50");
  EXPECT_TRUE(Details::UpdateFromProto(proto_with_price, &details));
  EXPECT_THAT(details.descriptionLine3(), Eq(""));
}

TEST_F(DetailsTest, GetPriceAttribution) {
  Details details;

  ShowDetailsProto proto_no_price;
  proto_no_price.mutable_details()->set_title("title");
  proto_no_price.mutable_details()->set_description_line_3("line 3");
  EXPECT_TRUE(Details::UpdateFromProto(proto_no_price, &details));
  EXPECT_THAT(details.priceAttribution(), Eq(""));

  ShowDetailsProto proto_with_price;
  proto_with_price.mutable_details()->set_title("title");
  proto_with_price.mutable_details()->set_description_line_3("Est. total");
  proto_with_price.mutable_details()->set_total_price("$2.50");
  EXPECT_TRUE(Details::UpdateFromProto(proto_with_price, &details));
  EXPECT_THAT(details.priceAttribution(), Eq("Est. total"));
}

TEST_F(DetailsTest, GetTitle) {
  Details details;
  ShowDetailsProto proto;
  proto.mutable_details()->set_title("title");
  EXPECT_TRUE(Details::UpdateFromProto(proto, &details));
  EXPECT_THAT(details.title(), Eq("title"));
}

TEST_F(DetailsTest, GetImageUrl) {
  Details details;
  ShowDetailsProto proto;
  proto.mutable_details()->set_image_url("url");
  EXPECT_TRUE(Details::UpdateFromProto(proto, &details));
  EXPECT_THAT(details.imageUrl(), Eq("url"));
}

TEST_F(DetailsTest, GetImageAccessibilityHint) {
  Details details;
  ShowDetailsProto proto;
  proto.mutable_details()->set_image_accessibility_hint("hint");
  EXPECT_TRUE(Details::UpdateFromProto(proto, &details));
  EXPECT_THAT(details.imageAccessibilityHint(), Eq("hint"));
}

TEST_F(DetailsTest, GetClickthroughData) {
  Details details;
  ShowDetailsProto proto;
  auto* clitkthrough_data =
      proto.mutable_details()->mutable_image_clickthrough_data();
  clitkthrough_data->set_allow_clickthrough(true);
  clitkthrough_data->set_description("description");
  clitkthrough_data->set_positive_text("positive");
  clitkthrough_data->set_negative_text("negative");
  proto.mutable_details()
      ->mutable_image_clickthrough_data()
      ->set_clickthrough_url("url");
  EXPECT_TRUE(Details::UpdateFromProto(proto, &details));
  EXPECT_TRUE(details.imageAllowClickthrough());
  EXPECT_THAT(details.imageDescription(), Eq("description"));
  EXPECT_THAT(details.imagePositiveText(), Eq("positive"));
  EXPECT_THAT(details.imageNegativeText(), Eq("negative"));
  EXPECT_THAT(details.imageClickthroughUrl(), Eq("url"));
}

TEST_F(DetailsTest, GetPlaceholderFlags) {
  Details details;
  ShowDetailsProto proto;
  proto.mutable_details()->set_show_image_placeholder(true);
  proto.mutable_details()->set_animate_placeholders(true);
  EXPECT_TRUE(Details::UpdateFromProto(proto, &details));
  EXPECT_TRUE(details.showImagePlaceholder());
  EXPECT_TRUE(details.animatePlaceholders());
}

TEST_F(DetailsTest, GetTotalPrice) {
  Details details;
  ShowDetailsProto proto;
  proto.mutable_details()->set_total_price_label("Total");
  proto.mutable_details()->set_total_price("$2.50");
  EXPECT_TRUE(Details::UpdateFromProto(proto, &details));
  EXPECT_THAT(details.totalPriceLabel(), Eq("Total"));
  EXPECT_THAT(details.totalPrice(), Eq("$2.50"));
}

TEST_F(DetailsTest, GetHighlightFlags) {
  Details details;
  ShowDetailsProto proto;
  proto.mutable_details()->set_title("title");
  proto.mutable_change_flags()->set_user_approval_required(true);
  proto.mutable_change_flags()->set_highlight_title(true);
  proto.mutable_change_flags()->set_highlight_line1(true);
  proto.mutable_change_flags()->set_highlight_line2(true);
  proto.mutable_change_flags()->set_highlight_line3(true);
  EXPECT_TRUE(Details::UpdateFromProto(proto, &details));
  EXPECT_TRUE(details.userApprovalRequired());
  EXPECT_TRUE(details.highlightTitle());
  EXPECT_TRUE(details.highlightLine1());
  EXPECT_TRUE(details.highlightLine2());
  EXPECT_TRUE(details.highlightLine3());
}

}  // namespace
}  // namespace autofill_assistant
