// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/details.h"

#include <unordered_set>

#include <base/strings/stringprintf.h>
#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

// TODO(b/141850276): Remove hardcoded formatting strings.
constexpr char kDateFormat[] = "EEE, MMM d";
constexpr char kTimeFormat[] = "h:mm a";
constexpr char kDateTimeSeparator[] = " \xE2\x80\xA2 ";
constexpr char kSpaceBetweenCardNumAndDate[] = "    ";

// Parse RFC 3339 date-time. Store the value in the datetime proto field.
bool ParseDateTimeStringToProto(const std::string& datetime,
                                DateTimeProto* result) {
  // RFC 3339 format without timezone: yyyy'-'MM'-'dd'T'HH':'mm':'ss
  std::string pattern =
      R"rgx((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2}))rgx";

  int year, month, day, hour, minute, second;
  if (re2::RE2::FullMatch(datetime, pattern, &year, &month, &day, &hour,
                          &minute, &second)) {
    auto* date = result->mutable_date();
    date->set_year(year);
    date->set_month(month);
    date->set_day(day);
    auto* time = result->mutable_time();
    time->set_hour(hour);
    time->set_minute(minute);
    time->set_second(second);
    return true;
  } else {
    return false;
  }
}

// Format a datetime proto with current locale.
std::string FormatDateTimeProto(const DateTimeProto& date_time) {
  if (!date_time.has_date() || !date_time.has_time()) {
    return std::string();
  }
  auto date_proto = date_time.date();
  auto time_proto = date_time.time();

  base::Time::Exploded exploded_time = {
      date_proto.year(),      date_proto.month(),
      /* day_of_week = */ -1, date_proto.day(),    time_proto.hour(),
      time_proto.minute(),    time_proto.second(), 0};
  base::Time time;

  if (base::Time::FromLocalExploded(exploded_time, &time)) {
    auto date_string = base::TimeFormatWithPattern(time, kDateFormat);
    auto time_string = base::TimeFormatWithPattern(time, kTimeFormat);

    return base::StrCat({base::UTF16ToUTF8(time_string), kDateTimeSeparator,
                         base::UTF16ToUTF8(date_string)});
  }

  return std::string();
}

// This logic is from NameInfo::FullName.
base::string16 FullName(const autofill::AutofillProfile& profile) {
  return autofill::data_util::JoinNameParts(
      profile.GetRawInfo(autofill::NAME_FIRST),
      profile.GetRawInfo(autofill::NAME_MIDDLE),
      profile.GetRawInfo(autofill::NAME_LAST));
}

}  // namespace

Details::Details() = default;
Details::~Details() = default;

// static
bool Details::UpdateFromProto(const ShowDetailsProto& proto, Details* details) {
  if (!proto.has_details()) {
    return false;
  }

  ShowDetailsProto updated_proto = proto;
  // Legacy treatment for old proto fields. Can be removed once the backend
  // is updated to set the description_line_1/line_2 fields.
  // TODO(crbug.com/806868): Is this still needed?
  if (updated_proto.details().has_description() &&
      !updated_proto.details().has_description_line_2()) {
    updated_proto.mutable_details()->set_description_line_2(
        updated_proto.details().description());
  }
  details->SetDetailsProto(updated_proto.details());
  details->SetDetailsChangesProto(updated_proto.change_flags());
  return true;
}

// static
bool Details::UpdateFromContactDetails(
    const ShowDetailsProto& proto,
    const UserData* user_data,
    const CollectUserDataOptions* user_data_options,
    Details* details) {
  if (!user_data_options || !(user_data_options->request_payer_name ||
                              user_data_options->request_payer_email)) {
    return false;
  }

  std::string contact_details = proto.contact_details();
  if (!user_data->has_selected_address(contact_details)) {
    return false;
  }

  ShowDetailsProto updated_proto = proto;
  auto* profile = user_data->selected_address(contact_details);
  auto* details_proto = updated_proto.mutable_details();
  details_proto->set_title(
      l10n_util::GetStringUTF8(IDS_PAYMENTS_CONTACT_DETAILS_LABEL));
  if (user_data_options->request_payer_name) {
    details_proto->set_description_line_1(
        base::UTF16ToUTF8(FullName(*profile)));
  }
  if (user_data_options->request_payer_email) {
    details_proto->set_description_line_2(
        base::UTF16ToUTF8(profile->GetRawInfo(autofill::EMAIL_ADDRESS)));
  }
  details->SetDetailsProto(updated_proto.details());
  details->SetDetailsChangesProto(updated_proto.change_flags());
  return true;
}

// static
bool Details::UpdateFromShippingAddress(const ShowDetailsProto& proto,
                                        const UserData* user_data,
                                        Details* details) {
  std::string shipping_address = proto.shipping_address();
  if (!user_data->has_selected_address(shipping_address)) {
    return false;
  }

  ShowDetailsProto updated_proto = proto;
  auto* profile = user_data->selected_address(shipping_address);
  auto* details_proto = updated_proto.mutable_details();
  autofill::CountryNames* country_names = autofill::CountryNames::GetInstance();
  details_proto->set_title(
      l10n_util::GetStringUTF8(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL));
  details_proto->set_description_line_1(base::UTF16ToUTF8(FullName(*profile)));
  details_proto->set_description_line_2(base::StrCat({
      base::UTF16ToUTF8(
          profile->GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS)),
      " ",
      base::UTF16ToUTF8(profile->GetRawInfo(autofill::ADDRESS_HOME_ZIP)),
      " ",
      base::UTF16ToUTF8(profile->GetRawInfo(autofill::ADDRESS_HOME_CITY)),
      " ",
      country_names->GetCountryCode(
          profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY)),
  }));
  details->SetDetailsProto(updated_proto.details());
  details->SetDetailsChangesProto(updated_proto.change_flags());
  return true;
}

bool Details::UpdateFromSelectedCreditCard(const ShowDetailsProto& proto,
                                           const UserData* user_data,
                                           Details* details) {
  if (user_data->selected_card_.get() == nullptr || !proto.credit_card()) {
    return false;
  }

  ShowDetailsProto updated_proto = proto;
  auto* card = user_data->selected_card_.get();
  auto* details_proto = updated_proto.mutable_details();
  details_proto->set_title(
      l10n_util::GetStringUTF8(IDS_PAYMENTS_METHOD_OF_PAYMENT_LABEL));
  details_proto->set_description_line_1(
      base::StrCat({base::UTF16ToUTF8(card->ObfuscatedLastFourDigits()),
                    kSpaceBetweenCardNumAndDate,
                    base::UTF16ToUTF8(card->AbbreviatedExpirationDateForDisplay(
                        /* with_prefix = */ false))}));
  details->SetDetailsProto(updated_proto.details());
  details->SetDetailsChangesProto(updated_proto.change_flags());
  return true;
}

base::Value Details::GetDebugContext() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (!proto_.title().empty())
    dict.SetKey("title", base::Value(proto_.title()));

  if (!proto_.image_url().empty())
    dict.SetKey("image_url", base::Value(proto_.image_url()));

  if (proto_.has_image_accessibility_hint())
    dict.SetKey("image_accessibility_hint",
                base::Value(proto_.image_accessibility_hint()));

  if (!proto_.total_price().empty())
    dict.SetKey("total_price", base::Value(proto_.total_price()));

  if (!proto_.total_price_label().empty())
    dict.SetKey("total_price_label", base::Value(proto_.total_price_label()));

  if (!proto_.description_line_1().empty())
    dict.SetKey("description_line_1", base::Value(proto_.description_line_1()));

  if (!proto_.description_line_2().empty())
    dict.SetKey("description_line_2", base::Value(proto_.description_line_2()));

  if (!proto_.description_line_3().empty())
    dict.SetKey("description_line_3", base::Value(proto_.description_line_3()));

  if (proto_.has_datetime()) {
    dict.SetKey(
        "datetime",
        base::Value(base::StringPrintf(
            "%d-%02d-%02dT%02d:%02d:%02d",
            static_cast<int>(proto_.datetime().date().year()),
            proto_.datetime().date().month(), proto_.datetime().date().day(),
            proto_.datetime().time().hour(), proto_.datetime().time().minute(),
            proto_.datetime().time().second())));
  }

  dict.SetKey("user_approval_required",
              base::Value(change_flags_.user_approval_required()));
  dict.SetKey("highlight_title", base::Value(change_flags_.highlight_title()));
  dict.SetKey("highlight_line1", base::Value(change_flags_.highlight_line1()));
  dict.SetKey("highlight_line2", base::Value(change_flags_.highlight_line2()));
  dict.SetKey("highlight_line3", base::Value(change_flags_.highlight_line3()));
  dict.SetKey("highlight_line3", base::Value(change_flags_.highlight_line3()));

  return dict;
}

bool Details::UpdateFromParameters(const TriggerContext& context) {
  base::Optional<std::string> show_initial =
      context.GetParameter("DETAILS_SHOW_INITIAL");
  if (show_initial.value_or("true") == "false") {
    return false;
  }
  // Whenever details are updated from parameters we want to animate missing
  // data.
  proto_.set_animate_placeholders(true);
  proto_.set_show_image_placeholder(true);
  if (MaybeUpdateFromDetailsParameters(context)) {
    Update();
    return true;
  }

  // NOTE: The logic below is only needed for backward compatibility.
  // Remove once we always pass detail parameters.
  bool is_updated = false;
  base::Optional<std::string> movie_name =
      context.GetParameter("MOVIES_MOVIE_NAME");
  if (movie_name) {
    proto_.set_title(movie_name.value());
    is_updated = true;
  }

  base::Optional<std::string> theater_name =
      context.GetParameter("MOVIES_THEATER_NAME");
  if (theater_name) {
    proto_.set_description_line_2(theater_name.value());
    is_updated = true;
  }

  base::Optional<std::string> screening_datetime =
      context.GetParameter("MOVIES_SCREENING_DATETIME");
  if (screening_datetime &&
      ParseDateTimeStringToProto(screening_datetime.value(),
                                 proto_.mutable_datetime())) {
    is_updated = true;
  }

  Update();
  return is_updated;
}

bool Details::MaybeUpdateFromDetailsParameters(const TriggerContext& context) {
  bool details_updated = false;

  base::Optional<std::string> title = context.GetParameter("DETAILS_TITLE");
  if (title) {
    proto_.set_title(title.value());
    details_updated = true;
  }

  base::Optional<std::string> description_line_1 =
      context.GetParameter("DETAILS_DESCRIPTION_LINE_1");
  if (description_line_1) {
    proto_.set_description_line_1(description_line_1.value());
    details_updated = true;
  }

  base::Optional<std::string> description_line_2 =
      context.GetParameter("DETAILS_DESCRIPTION_LINE_2");
  if (description_line_2) {
    proto_.set_description_line_2(description_line_2.value());
    details_updated = true;
  }

  base::Optional<std::string> description_line_3 =
      context.GetParameter("DETAILS_DESCRIPTION_LINE_3");
  if (description_line_3) {
    proto_.set_description_line_3(description_line_3.value());
    details_updated = true;
  }

  base::Optional<std::string> image_url =
      context.GetParameter("DETAILS_IMAGE_URL");
  if (image_url) {
    proto_.set_image_url(image_url.value());
    details_updated = true;
  }

  base::Optional<std::string> image_accessibility_hint =
      context.GetParameter("DETAILS_IMAGE_ACCESSIBILITY_HINT");
  if (image_accessibility_hint) {
    proto_.set_image_accessibility_hint(image_accessibility_hint.value());
    details_updated = true;
  }

  base::Optional<std::string> image_clickthrough_url =
      context.GetParameter("DETAILS_IMAGE_CLICKTHROUGH_URL");
  if (image_clickthrough_url) {
    proto_.mutable_image_clickthrough_data()->set_allow_clickthrough(true);
    proto_.mutable_image_clickthrough_data()->set_clickthrough_url(
        image_clickthrough_url.value());
    details_updated = true;
  }

  base::Optional<std::string> total_price_label =
      context.GetParameter("DETAILS_TOTAL_PRICE_LABEL");
  if (total_price_label) {
    proto_.set_total_price_label(total_price_label.value());
    details_updated = true;
  }

  base::Optional<std::string> total_price =
      context.GetParameter("DETAILS_TOTAL_PRICE");
  if (total_price) {
    proto_.set_total_price(total_price.value());
    details_updated = true;
  }

  return details_updated;
}

void Details::SetDetailsProto(const DetailsProto& proto) {
  proto_ = proto;
  Update();
}

const std::string Details::title() const {
  return proto_.title();
}

int Details::titleMaxLines() const {
  return title_max_lines_;
}

const std::string Details::imageUrl() const {
  return proto_.image_url();
}

const base::Optional<std::string> Details::imageAccessibilityHint() const {
  if (proto_.has_image_accessibility_hint()) {
    return proto_.image_accessibility_hint();
  }
  return base::nullopt;
}

bool Details::imageAllowClickthrough() const {
  return proto_.image_clickthrough_data().allow_clickthrough();
}

const std::string Details::imageDescription() const {
  return proto_.image_clickthrough_data().description();
}

const std::string Details::imagePositiveText() const {
  return proto_.image_clickthrough_data().positive_text();
}

const std::string Details::imageNegativeText() const {
  return proto_.image_clickthrough_data().negative_text();
}

const std::string Details::imageClickthroughUrl() const {
  return proto_.image_clickthrough_data().clickthrough_url();
}

bool Details::showImagePlaceholder() const {
  return proto_.show_image_placeholder();
}

const std::string Details::totalPriceLabel() const {
  return proto_.total_price_label();
}

const std::string Details::totalPrice() const {
  return proto_.total_price();
}

const std::string Details::descriptionLine1() const {
  return description_line_1_content_;
}

const std::string Details::descriptionLine2() const {
  return proto_.description_line_2();
}

const std::string Details::descriptionLine3() const {
  return description_line_3_content_;
}

const std::string Details::priceAttribution() const {
  return price_attribution_content_;
}

bool Details::userApprovalRequired() const {
  return change_flags_.user_approval_required();
}

bool Details::highlightTitle() const {
  return change_flags_.highlight_title();
}

bool Details::highlightLine1() const {
  return change_flags_.highlight_line1();
}

bool Details::highlightLine2() const {
  return change_flags_.highlight_line2();
}

bool Details::highlightLine3() const {
  return change_flags_.highlight_line3();
}

bool Details::animatePlaceholders() const {
  return proto_.animate_placeholders();
}

void Details::ClearChanges() {
  change_flags_.Clear();
}

void Details::Update() {
  auto formatted_datetime = FormatDateTimeProto(proto_.datetime());
  description_line_1_content_.assign(proto_.description_line_1().empty()
                                         ? formatted_datetime
                                         : proto_.description_line_1());

  description_line_3_content_.assign(proto_.total_price().empty()
                                         ? proto_.description_line_3()
                                         : std::string());
  price_attribution_content_.assign(proto_.total_price().empty()
                                        ? std::string()
                                        : proto_.description_line_3());

  bool isDescriptionLine1Empty = descriptionLine1().empty();
  bool isDescriptionLine2Empty = descriptionLine2().empty();
  if (isDescriptionLine1Empty && isDescriptionLine2Empty) {
    title_max_lines_ = 3;
  } else if (isDescriptionLine1Empty || isDescriptionLine2Empty) {
    title_max_lines_ = 2;
  } else {
    title_max_lines_ = 1;
  }
}

}  // namespace autofill_assistant
