// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/suggestion.h"

#include <type_traits>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"

namespace autofill {

namespace {

std::string_view ConvertAcceptabilityToPrintableString(
  Suggestion::Acceptability acceptability) {
  switch (acceptability) {
    case Suggestion::Acceptability::kAcceptable:
      return "kAcceptable";
    case Suggestion::Acceptability::kUnacceptable:
      return "kUnacceptable";
    case Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle:
      return "kUnacceptableWithDeactivatedStyle";
  }
  NOTREACHED();
}

std::string_view ConvertIconToPrintableString(Suggestion::Icon icon) {
  switch (icon) {
    case Suggestion::Icon::kAccount:
      return "kAccount";
    case Suggestion::Icon::kClear:
      return "kClear";
    case Suggestion::Icon::kCreate:
      return "kCreate";
    case Suggestion::Icon::kCode:
      return "kCode";
    case Suggestion::Icon::kDelete:
      return "kDelete";
    case Suggestion::Icon::kDevice:
      return "kDevice";
    case Suggestion::Icon::kEdit:
      return "kEdit";
    case Suggestion::Icon::kEmail:
      return "kEmail";
    case Suggestion::Icon::kEmpty:
      return "kEmpty";
    case Suggestion::Icon::kError:
      return "kError";
    case Suggestion::Icon::kGlobe:
      return "kGlobe";
    case Suggestion::Icon::kGoogle:
      return "kGoogle";
    case Suggestion::Icon::kGoogleMonochrome:
      return "kGoogleMonochrome";
    case Suggestion::Icon::kGooglePasswordManager:
      return "kGooglePasswordManager";
    case Suggestion::Icon::kGooglePay:
      return "kGooglePay";
    case Suggestion::Icon::kGooglePayDark:
      return "kGooglePayDark";
    case Suggestion::Icon::kHttpWarning:
      return "kHttpWarning";
    case Suggestion::Icon::kHttpsInvalid:
      return "kHttpsInvalid";
    case Suggestion::Icon::kKey:
      return "kKey";
    case Suggestion::Icon::kLocation:
      return "kLocation";
    case Suggestion::Icon::kMagic:
      return "kMagic";
    case Suggestion::Icon::kOfferTag:
      return "kOfferTag";
    case Suggestion::Icon::kPenSpark:
      return "kPenSpark";
    case Suggestion::Icon::kScanCreditCard:
      return "kScanCreditCard";
    case Suggestion::Icon::kSettings:
      return "kSettings";
    case Suggestion::Icon::kAutofillPredictionImprovements:
      return "kAutofillPredictionImprovements";
    case Suggestion::Icon::kSettingsAndroid:
      return "kSettingsAndroid";
    case Suggestion::Icon::kUndo:
      return "kUndo";
    case Suggestion::Icon::kCardGeneric:
      return "kCardGeneric";
    case Suggestion::Icon::kCardAmericanExpress:
      return "kCardAmericanExpress";
    case Suggestion::Icon::kCardDiners:
      return "kCardDiners";
    case Suggestion::Icon::kCardDiscover:
      return "kCardDiscover";
    case Suggestion::Icon::kCardElo:
      return "kCardElo";
    case Suggestion::Icon::kCardJCB:
      return "kCardJCB";
    case Suggestion::Icon::kCardMasterCard:
      return "kCardMasterCard";
    case Suggestion::Icon::kCardMir:
      return "kCardMir";
    case Suggestion::Icon::kCardTroy:
      return "kCardTroy";
    case Suggestion::Icon::kCardUnionPay:
      return "kCardUnionPay";
    case Suggestion::Icon::kCardVerve:
      return "kCardVerve";
    case Suggestion::Icon::kCardVisa:
      return "kCardVisa";
    case Suggestion::Icon::kIban:
      return "kIban";
    case Suggestion::Icon::kPlusAddress:
      return "kPlusAddress";
    case Suggestion::Icon::kNoIcon:
      return "kNoIcon";
  }
  NOTREACHED();
}

} // namespace

Suggestion::PasswordSuggestionDetails::PasswordSuggestionDetails() = default;
Suggestion::PasswordSuggestionDetails::PasswordSuggestionDetails(
    std::u16string_view username,
    std::u16string_view password,
    std::string_view signon_realm,
    std::u16string_view display_signon_realm,
    bool is_cross_domain)
    : username(username),
      password(password),
      signon_realm(signon_realm),
      display_signon_realm(display_signon_realm),
      is_cross_domain(is_cross_domain) {}

Suggestion::PasswordSuggestionDetails::PasswordSuggestionDetails(
    const PasswordSuggestionDetails&) = default;
Suggestion::PasswordSuggestionDetails::PasswordSuggestionDetails(
    PasswordSuggestionDetails&) = default;
Suggestion::PasswordSuggestionDetails&
Suggestion::PasswordSuggestionDetails::operator=(
    const PasswordSuggestionDetails&) = default;
Suggestion::PasswordSuggestionDetails&
Suggestion::PasswordSuggestionDetails::operator=(PasswordSuggestionDetails&&) =
    default;
Suggestion::PasswordSuggestionDetails::~PasswordSuggestionDetails() = default;

Suggestion::PlusAddressPayload::PlusAddressPayload() = default;

Suggestion::PlusAddressPayload::PlusAddressPayload(
    std::optional<std::u16string> address)
    : address(std::move(address)) {}

Suggestion::PlusAddressPayload::PlusAddressPayload(const PlusAddressPayload&) =
    default;

Suggestion::PlusAddressPayload::PlusAddressPayload(PlusAddressPayload&&) =
    default;

Suggestion::PlusAddressPayload& Suggestion::PlusAddressPayload::operator=(
    const PlusAddressPayload&) = default;

Suggestion::PlusAddressPayload& Suggestion::PlusAddressPayload::operator=(
    PlusAddressPayload&&) = default;

Suggestion::PlusAddressPayload::~PlusAddressPayload() = default;

Suggestion::PredictionImprovementsPayload::PredictionImprovementsPayload() =
    default;

Suggestion::PredictionImprovementsPayload::PredictionImprovementsPayload(
    const base::flat_map<FieldGlobalId, std::u16string>& values_to_fill,
    const FieldTypeSet& field_types_to_fill,
    const DenseSet<FieldFillingSkipReason>& ignorable_skip_reasons)
    : values_to_fill(std::move(values_to_fill)),
      field_types_to_fill(std::move(field_types_to_fill)),
      ignorable_skip_reasons(std::move(ignorable_skip_reasons)) {}

Suggestion::PredictionImprovementsPayload::PredictionImprovementsPayload(
    const PredictionImprovementsPayload&) = default;

Suggestion::PredictionImprovementsPayload::PredictionImprovementsPayload(
    PredictionImprovementsPayload&&) = default;

Suggestion::PredictionImprovementsPayload&
Suggestion::PredictionImprovementsPayload::operator=(
    const PredictionImprovementsPayload&) = default;

Suggestion::PredictionImprovementsPayload&
Suggestion::PredictionImprovementsPayload::operator=(
    PredictionImprovementsPayload&&) = default;

Suggestion::PredictionImprovementsPayload::~PredictionImprovementsPayload() =
    default;

Suggestion::AutofillProfilePayload::AutofillProfilePayload() = default;
Suggestion::AutofillProfilePayload::AutofillProfilePayload(Guid guid)
    : AutofillProfilePayload(std::move(guid), u"") {}
Suggestion::AutofillProfilePayload::AutofillProfilePayload(
    Guid guid,
    std::u16string email_override)
    : guid(std::move(guid)), email_override(std::move(email_override)) {}

Suggestion::AutofillProfilePayload::AutofillProfilePayload(
    const AutofillProfilePayload&) = default;

Suggestion::AutofillProfilePayload::AutofillProfilePayload(
    AutofillProfilePayload&&) = default;

Suggestion::AutofillProfilePayload&
Suggestion::AutofillProfilePayload::operator=(const AutofillProfilePayload&) =
    default;

Suggestion::AutofillProfilePayload&
Suggestion::AutofillProfilePayload::operator=(AutofillProfilePayload&&) =
    default;

Suggestion::AutofillProfilePayload::~AutofillProfilePayload() = default;

Suggestion::PaymentsPayload::PaymentsPayload() = default;

Suggestion::PaymentsPayload::PaymentsPayload(
    std::u16string main_text_content_description,
    bool should_display_terms_available)
    : main_text_content_description(main_text_content_description),
      should_display_terms_available(should_display_terms_available) {}

Suggestion::PaymentsPayload::PaymentsPayload(const PaymentsPayload&) = default;

Suggestion::PaymentsPayload::PaymentsPayload(PaymentsPayload&&) = default;

Suggestion::PaymentsPayload& Suggestion::PaymentsPayload::operator=(
    const PaymentsPayload&) = default;

Suggestion::PaymentsPayload& Suggestion::PaymentsPayload::operator=(
    PaymentsPayload&&) = default;

Suggestion::PaymentsPayload::~PaymentsPayload() = default;

Suggestion::IPHMetadata::IPHMetadata() = default;

Suggestion::IPHMetadata::IPHMetadata(const base::Feature* feature,
                                     std::vector<std::u16string> iph_params)
    : feature(feature), iph_params(std::move(iph_params)) {}

Suggestion::IPHMetadata::IPHMetadata(const IPHMetadata& other) = default;
Suggestion::IPHMetadata::IPHMetadata(IPHMetadata&& other) = default;

Suggestion::IPHMetadata& Suggestion::IPHMetadata::operator=(
    const IPHMetadata& other) = default;
Suggestion::IPHMetadata& Suggestion::IPHMetadata::operator=(
    IPHMetadata&& other) = default;

Suggestion::IPHMetadata::~IPHMetadata() = default;

Suggestion::Text::Text() = default;

Suggestion::Text::Text(std::u16string value,
                       IsPrimary is_primary,
                       ShouldTruncate should_truncate)
    : value(value), is_primary(is_primary), should_truncate(should_truncate) {}

Suggestion::Text::Text(const Text& other) = default;
Suggestion::Text::Text(Text& other) = default;

Suggestion::Text& Suggestion::Text::operator=(const Text& other) = default;
Suggestion::Text& Suggestion::Text::operator=(Text&& other) = default;

Suggestion::Text::~Text() = default;

Suggestion::Suggestion() = default;

Suggestion::Suggestion(std::u16string main_text)
    : main_text(std::move(main_text), Text::IsPrimary(true)) {}

Suggestion::Suggestion(SuggestionType type) : type(type) {}

Suggestion::Suggestion(std::u16string main_text, SuggestionType type)
    : type(type), main_text(std::move(main_text), Text::IsPrimary(true)) {}

Suggestion::Suggestion(std::string_view main_text,
                       std::string_view label,
                       Icon icon,
                       SuggestionType type)
    : type(type),
      main_text(base::UTF8ToUTF16(main_text), Text::IsPrimary(true)),
      icon(icon) {
  if (!label.empty())
    this->labels = {{Text(base::UTF8ToUTF16(label))}};
}

Suggestion::Suggestion(std::string_view main_text,
                       std::vector<std::vector<Text>> labels,
                       Icon icon,
                       SuggestionType type)
    : type(type),
      main_text(base::UTF8ToUTF16(main_text), Text::IsPrimary(true)),
      labels(std::move(labels)),
      icon(icon) {}

Suggestion::Suggestion(std::string_view main_text,
                       std::string_view minor_text,
                       std::string_view label,
                       Icon icon,
                       SuggestionType type)
    : type(type),
      main_text(base::UTF8ToUTF16(main_text), Text::IsPrimary(true)),
      minor_text(base::UTF8ToUTF16(minor_text)),
      icon(icon) {
  if (!label.empty())
    this->labels = {{Text(base::UTF8ToUTF16(label))}};
}

Suggestion::Suggestion(const Suggestion& other) = default;
Suggestion::Suggestion(Suggestion&& other) = default;

Suggestion& Suggestion::operator=(const Suggestion& other) = default;
Suggestion& Suggestion::operator=(Suggestion&& other) = default;

Suggestion::~Suggestion() = default;

bool Suggestion::IsAcceptable() const {
  switch (acceptability) {
    case Acceptability::kAcceptable:
      return true;
    case Acceptability::kUnacceptable:
    case Acceptability::kUnacceptableWithDeactivatedStyle:
      return false;
  }
  NOTREACHED();
}

bool Suggestion::HasDeactivatedStyle() const {
  switch (acceptability) {
    case Acceptability::kAcceptable:
    case Acceptability::kUnacceptable:
      return false;
    case Acceptability::kUnacceptableWithDeactivatedStyle:
      return true;
  }
  NOTREACHED();
}

void PrintTo(const Suggestion& suggestion, std::ostream* os) {
  *os << std::endl
      << "Suggestion (type:" << suggestion.type << ", main_text:\""
      << suggestion.main_text.value << "\""
      << (suggestion.main_text.is_primary ? "(Primary)" : "(Not Primary)")
      << ", minor_text:\"" << suggestion.minor_text.value << "\""
      << (suggestion.minor_text.is_primary ? "(Primary)" : "(Not Primary)")
      << ", additional_label: \"" << suggestion.additional_label << "\""
      << ", acceptability: \""
      << ConvertAcceptabilityToPrintableString(suggestion.acceptability) << "\""
      << ", icon:" << ConvertIconToPrintableString(suggestion.icon)
      << ", trailing_icon:"
      << ConvertIconToPrintableString(suggestion.trailing_icon) << ")";
}

}  // namespace autofill
