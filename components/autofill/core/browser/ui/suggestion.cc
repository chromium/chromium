// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/suggestion.h"

#include <type_traits>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"

namespace autofill {

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

bool Suggestion::Text::operator==(const Suggestion::Text& text) const {
  return value == text.value && is_primary == text.is_primary &&
         should_truncate == text.should_truncate;
}

bool Suggestion::Text::operator!=(const Suggestion::Text& text) const {
  return !operator==(text);
}

// static
Suggestion::Icon Suggestion::ConvertIconStringIntoIcon(
    std::string_view icon_string) {
  if (icon_string == kAmericanExpressCard) {
    return Suggestion::Icon::kCardAmericanExpress;
  }
  if (icon_string == kDinersCard) {
    return Suggestion::Icon::kCardDiners;
  }
  if (icon_string == kDiscoverCard) {
    return Suggestion::Icon::kCardDiscover;
  }
  if (icon_string == kEloCard) {
    return Suggestion::Icon::kCardElo;
  }
  if (icon_string == kJCBCard) {
    return Suggestion::Icon::kCardJCB;
  }
  if (icon_string == kMasterCard) {
    return Suggestion::Icon::kCardMasterCard;
  }
  if (icon_string == kMirCard) {
    return Suggestion::Icon::kCardMir;
  }
  if (icon_string == kTroyCard) {
    return Suggestion::Icon::kCardTroy;
  }
  if (icon_string == kUnionPay) {
    return Suggestion::Icon::kCardUnionPay;
  }
  if (icon_string == kVisaCard) {
    return Suggestion::Icon::kCardVisa;
  }
  if (icon_string == kGenericCard) {
    return Suggestion::Icon::kCardGeneric;
  }

  if (icon_string == "accountIcon") {
    return Suggestion::Icon::kAccount;
  }
  if (icon_string == "clearIcon") {
    return Suggestion::Icon::kClear;
  }
  if (icon_string == "create") {
    return Suggestion::Icon::kCreate;
  }
  if (icon_string == "codeIcon") {
    return Suggestion::Icon::kCode;
  }
  if (icon_string == "deleteIcon") {
    return Suggestion::Icon::kDelete;
  }
  if (icon_string == "device") {
    return Suggestion::Icon::kDevice;
  }
  if (icon_string == "editIcon") {
    return Suggestion::Icon::kEdit;
  }
  if (icon_string == "empty") {
    return Suggestion::Icon::kEmpty;
  }
  if (icon_string == "globeIcon") {
    return Suggestion::Icon::kGlobe;
  }
  if (icon_string == "google") {
    return Suggestion::Icon::kGoogle;
  }
  if (icon_string == "googlePasswordManager") {
    return Suggestion::Icon::kGooglePasswordManager;
  }
  if (icon_string == "googlePay") {
    return Suggestion::Icon::kGooglePay;
  }
  if (icon_string == "googlePayDark") {
    return Suggestion::Icon::kGooglePayDark;
  }
  if (icon_string == "httpWarning") {
    return Suggestion::Icon::kHttpWarning;
  }
  if (icon_string == "httpsInvalid") {
    return Suggestion::Icon::kHttpsInvalid;
  }
  if (icon_string == "keyIcon") {
    return Suggestion::Icon::kKey;
  }
  if (icon_string == "locationIcon") {
    return Suggestion::Icon::kLocation;
  }
  if (icon_string == "magicIcon") {
    return Suggestion::Icon::kMagic;
  }
  if (icon_string == "offerTag") {
    return Suggestion::Icon::kOfferTag;
  }
  if (icon_string == "penSparkIcon") {
    return Suggestion::Icon::kPenSpark;
  }
  if (icon_string == "scanCreditCardIcon") {
    return Suggestion::Icon::kScanCreditCard;
  }
  if (icon_string == "settingsIcon") {
    return Suggestion::Icon::kSettings;
  }
  if (icon_string == "settings") {
    return Suggestion::Icon::kSettingsAndroid;
  }
  if (icon_string == "undoIcon") {
    return Suggestion::Icon::kUndo;
  }
  if (icon_string == "") {
    return Suggestion::Icon::kNoIcon;
  }
  NOTREACHED_NORETURN();
}

Suggestion::Suggestion() = default;

Suggestion::Suggestion(std::u16string main_text)
    : main_text(std::move(main_text), Text::IsPrimary(true)) {}

Suggestion::Suggestion(PopupItemId popup_item_id)
    : popup_item_id(popup_item_id) {}

Suggestion::Suggestion(std::u16string main_text, PopupItemId popup_item_id)
    : popup_item_id(popup_item_id),
      main_text(std::move(main_text), Text::IsPrimary(true)) {}

// TODO(crbug.com/1019660): Make this constructor accept Icon enum.
Suggestion::Suggestion(base::StringPiece main_text,
                       base::StringPiece label,
                       std::string_view icon_str,
                       PopupItemId popup_item_id)
    : popup_item_id(popup_item_id),
      main_text(base::UTF8ToUTF16(main_text), Text::IsPrimary(true)),
      icon(ConvertIconStringIntoIcon(icon_str)) {
  if (!label.empty())
    this->labels = {{Text(base::UTF8ToUTF16(label))}};
}

Suggestion::Suggestion(base::StringPiece main_text,
                       base::StringPiece minor_text,
                       base::StringPiece label,
                       Icon icon,
                       PopupItemId popup_item_id)
    : popup_item_id(popup_item_id),
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
    case Suggestion::Icon::kEmpty:
      return "kEmpty";
    case Suggestion::Icon::kGlobe:
      return "kGlobe";
    case Suggestion::Icon::kGoogle:
      return "kGoogle";
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
    case Suggestion::Icon::kCardVisa:
      return "kCardVisa";
    case Suggestion::Icon::kNoIcon:
      return "kNoIcon";
    default:
      NOTREACHED_NORETURN();
  }
}

void PrintTo(const Suggestion& suggestion, std::ostream* os) {
  *os << std::endl
      << "Suggestion (popup_item_id:"
      << base::to_underlying(suggestion.popup_item_id) << ", main_text:\""
      << suggestion.main_text.value << "\""
      << (suggestion.main_text.is_primary ? "(Primary)" : "(Not Primary)")
      << ", minor_text:\"" << suggestion.minor_text.value << "\""
      << (suggestion.minor_text.is_primary ? "(Primary)" : "(Not Primary)")
      << ", additional_label: \"" << suggestion.additional_label << "\""
      << ", icon:" << ConvertIconToPrintableString(suggestion.icon)
      << ", trailing_icon:"
      << ConvertIconToPrintableString(suggestion.trailing_icon) << ")";
}

}  // namespace autofill
