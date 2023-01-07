// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/accessory_sheet_data.h"

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"

namespace autofill {

AccessorySheetField::AccessorySheetField(std::u16string display_text,
                                         std::u16string text_to_fill,
                                         std::u16string a11y_description,
                                         std::string id,
                                         bool is_obfuscated,
                                         bool selectable)
    : display_text_(std::move(display_text)),
      text_to_fill_(std::move(text_to_fill)),
      a11y_description_(std::move(a11y_description)),
      id_(std::move(id)),
      is_obfuscated_(is_obfuscated),
      selectable_(selectable),
      estimated_memory_use_by_strings_(
          base::trace_event::EstimateMemoryUsage(display_text_) +
          base::trace_event::EstimateMemoryUsage(text_to_fill_) +
          base::trace_event::EstimateMemoryUsage(a11y_description_) +
          base::trace_event::EstimateMemoryUsage(id_)) {}

AccessorySheetField::AccessorySheetField(const AccessorySheetField& field) =
    default;

AccessorySheetField::AccessorySheetField(AccessorySheetField&& field) = default;

AccessorySheetField::~AccessorySheetField() = default;

AccessorySheetField& AccessorySheetField::operator=(
    const AccessorySheetField& field) = default;

AccessorySheetField& AccessorySheetField::operator=(
    AccessorySheetField&& field) = default;

bool AccessorySheetField::operator==(const AccessorySheetField& field) const {
  return display_text_ == field.display_text_ &&
         text_to_fill_ == field.text_to_fill_ &&
         a11y_description_ == field.a11y_description_ && id_ == field.id_ &&
         is_obfuscated_ == field.is_obfuscated_ &&
         selectable_ == field.selectable_;
}

size_t AccessorySheetField::EstimateMemoryUsage() const {
  return sizeof(AccessorySheetField) + estimated_memory_use_by_strings_;
}

std::ostream& operator<<(std::ostream& os, const AccessorySheetField& field) {
  os << "(display text: \"" << field.display_text() << "\", "
     << "text_to_fill: \"" << field.text_to_fill() << "\", "
     << "a11y_description: \"" << field.a11y_description() << "\", "
     << "id: \"" << field.id() << "\", "
     << "is " << (field.selectable() ? "" : "not ") << "selectable, "
     << "is " << (field.is_obfuscated() ? "" : "not ") << "obfuscated)";
  return os;
}

UserInfo::UserInfo() = default;

UserInfo::UserInfo(std::string origin)
    : UserInfo(std::move(origin), IsExactMatch(true)) {}

UserInfo::UserInfo(std::string origin, IsExactMatch is_exact_match)
    : UserInfo(std::move(origin), is_exact_match, GURL()) {}

UserInfo::UserInfo(std::string origin, GURL icon_url)
    : UserInfo(std::move(origin), IsExactMatch(true), std::move(icon_url)) {}

UserInfo::UserInfo(std::string origin,
                   IsExactMatch is_exact_match,
                   GURL icon_url)
    : origin_(std::move(origin)),
      is_exact_match_(is_exact_match),
      icon_url_(std::move(icon_url)),
      estimated_dynamic_memory_use_(
          base::trace_event::EstimateMemoryUsage(origin_) +
          base::trace_event::EstimateMemoryUsage(icon_url_)) {}

UserInfo::UserInfo(const UserInfo& user_info) = default;

UserInfo::UserInfo(UserInfo&& field) = default;

UserInfo::~UserInfo() = default;

UserInfo& UserInfo::operator=(const UserInfo& user_info) = default;

UserInfo& UserInfo::operator=(UserInfo&& user_info) = default;

bool UserInfo::operator==(const UserInfo& user_info) const {
  return fields_ == user_info.fields_ && origin_ == user_info.origin_ &&
         is_exact_match_ == user_info.is_exact_match_ &&
         icon_url_ == user_info.icon_url_;
}

size_t UserInfo::EstimateMemoryUsage() const {
  return sizeof(UserInfo) + estimated_dynamic_memory_use_;
}

std::ostream& operator<<(std::ostream& os, const UserInfo& user_info) {
  os << "origin: \"" << user_info.origin() << "\", "
     << "is_exact_match: " << std::boolalpha << user_info.is_exact_match()
     << ", "
     << "icon_url: " << user_info.icon_url() << ","
     << "fields: [\n";
  for (const AccessorySheetField& field : user_info.fields()) {
    os << field << ", \n";
  }
  return os << "]";
}

PromoCodeInfo::PromoCodeInfo(std::u16string promo_code,
                             std::u16string details_text)
    : promo_code_(AccessorySheetField(/*display_text=*/promo_code,
                                      /*text_to_fill=*/promo_code,
                                      /*a11y_description=*/promo_code,
                                      /*id=*/std::string(),
                                      /*is_password=*/false,
                                      /*selectable=*/true)),
      details_text_(details_text),
      estimated_dynamic_memory_use_(
          base::trace_event::EstimateMemoryUsage(promo_code_) +
          base::trace_event::EstimateMemoryUsage(details_text_)) {}

PromoCodeInfo::PromoCodeInfo(const PromoCodeInfo& promo_code_info) = default;

PromoCodeInfo::PromoCodeInfo(PromoCodeInfo&& promo_code_info) = default;

PromoCodeInfo::~PromoCodeInfo() = default;

PromoCodeInfo& PromoCodeInfo::operator=(const PromoCodeInfo& promo_code_info) =
    default;

PromoCodeInfo& PromoCodeInfo::operator=(PromoCodeInfo&& promo_code_info) =
    default;

bool PromoCodeInfo::operator==(const PromoCodeInfo& promo_code_info) const {
  return promo_code_ == promo_code_info.promo_code_ &&
         details_text_ == promo_code_info.details_text_;
}

size_t PromoCodeInfo::EstimateMemoryUsage() const {
  return sizeof(PromoCodeInfo) + estimated_dynamic_memory_use_;
}

std::ostream& operator<<(std::ostream& os,
                         const PromoCodeInfo& promo_code_info) {
  os << "promo_code: \"" << promo_code_info.promo_code() << "\", "
     << "details_text: \"" << promo_code_info.details_text() << "\"";
  return os;
}

FooterCommand::FooterCommand(std::u16string display_text,
                             AccessoryAction action)
    : display_text_(std::move(display_text)),
      accessory_action_(action),
      estimated_memory_use_by_strings_(
          base::trace_event::EstimateMemoryUsage(display_text_)) {}

FooterCommand::FooterCommand(const FooterCommand& footer_command) = default;

FooterCommand::FooterCommand(FooterCommand&& footer_command) = default;

FooterCommand::~FooterCommand() = default;

FooterCommand& FooterCommand::operator=(const FooterCommand& footer_command) =
    default;

FooterCommand& FooterCommand::operator=(FooterCommand&& footer_command) =
    default;

bool FooterCommand::operator==(const FooterCommand& fc) const {
  return display_text_ == fc.display_text_ &&
         accessory_action_ == fc.accessory_action_;
}

size_t FooterCommand::EstimateMemoryUsage() const {
  return sizeof(FooterCommand) + estimated_memory_use_by_strings_;
}

std::ostream& operator<<(std::ostream& os, const FooterCommand& fc) {
  return os << "(display text: \"" << fc.display_text() << "\", "
            << "action: " << static_cast<int>(fc.accessory_action()) << ")";
}

OptionToggle::OptionToggle(std::u16string display_text,
                           bool enabled,
                           AccessoryAction action)
    : display_text_(display_text),
      enabled_(enabled),
      accessory_action_(action),
      estimated_memory_use_by_strings_(
          base::trace_event::EstimateMemoryUsage(display_text_)) {}

OptionToggle::OptionToggle(const OptionToggle& option_toggle) = default;

OptionToggle::OptionToggle(OptionToggle&& option_toggle) = default;

OptionToggle::~OptionToggle() = default;

OptionToggle& OptionToggle::operator=(const OptionToggle& option_toggle) =
    default;

OptionToggle& OptionToggle::operator=(OptionToggle&& option_toggle) = default;

bool OptionToggle::operator==(const OptionToggle& option_toggle) const {
  return display_text_ == option_toggle.display_text_ &&
         enabled_ == option_toggle.enabled_ &&
         accessory_action_ == option_toggle.accessory_action_;
}

size_t OptionToggle::EstimateMemoryUsage() const {
  return sizeof(OptionToggle) + estimated_memory_use_by_strings_;
}

std::ostream& operator<<(std::ostream& os, const OptionToggle& ot) {
  return os << "(display text: \"" << ot.display_text() << "\", "
            << "state: " << ot.is_enabled() << ", "
            << "action: " << static_cast<int>(ot.accessory_action()) << ")";
}

std::ostream& operator<<(std::ostream& os, const AccessoryTabType& type) {
  switch (type) {
    case AccessoryTabType::PASSWORDS:
      return os << "Passwords sheet";
    case AccessoryTabType::CREDIT_CARDS:
      return os << "Payments sheet";
    case AccessoryTabType::ADDRESSES:
      return os << "Address sheet";
    case AccessoryTabType::OBSOLETE_TOUCH_TO_FILL:
      return os << "(obsolete) Touch to Fill sheet";
    case AccessoryTabType::ALL:
      return os << "All sheets";
    case AccessoryTabType::COUNT:
      return os << "Invalid sheet";
  }
  return os;
}

AccessorySheetData::AccessorySheetData(AccessoryTabType sheet_type,
                                       std::u16string title)
    : AccessorySheetData(sheet_type, std::move(title), std::u16string()) {}
AccessorySheetData::AccessorySheetData(AccessoryTabType sheet_type,
                                       std::u16string title,
                                       std::u16string warning)
    : sheet_type_(sheet_type),
      title_(std::move(title)),
      warning_(std::move(warning)) {}

AccessorySheetData::AccessorySheetData(const AccessorySheetData& data) =
    default;

AccessorySheetData::AccessorySheetData(AccessorySheetData&& data) = default;

AccessorySheetData::~AccessorySheetData() = default;

AccessorySheetData& AccessorySheetData::operator=(
    const AccessorySheetData& data) = default;

AccessorySheetData& AccessorySheetData::operator=(AccessorySheetData&& data) =
    default;

bool AccessorySheetData::operator==(const AccessorySheetData& data) const {
  return sheet_type_ == data.sheet_type_ && title_ == data.title_ &&
         warning_ == data.warning_ && option_toggle_ == data.option_toggle_ &&
         user_info_list_ == data.user_info_list_ &&
         promo_code_info_list_ == data.promo_code_info_list_ &&
         footer_commands_ == data.footer_commands_;
}

size_t AccessorySheetData::EstimateMemoryUsage() const {
  return sizeof(AccessorySheetData) +
         base::trace_event::EstimateMemoryUsage(title_) +
         base::trace_event::EstimateMemoryUsage(warning_) +
         (option_toggle_
              ? base::trace_event::EstimateMemoryUsage(option_toggle_.value())
              : 0) +
         base::trace_event::EstimateIterableMemoryUsage(user_info_list_) +
         base::trace_event::EstimateIterableMemoryUsage(promo_code_info_list_) +
         base::trace_event::EstimateIterableMemoryUsage(footer_commands_);
}

std::ostream& operator<<(std::ostream& os, const AccessorySheetData& data) {
  os << data.get_sheet_type() << " with title: \"" << data.title();
  if (data.option_toggle().has_value()) {
    os << "\", with option toggle: \"" << data.option_toggle().value();
  } else {
    os << "\", with option toggle: \"none";
  }

  os << "\", warning: \"" << data.warning() << "\", and user info list: [";
  for (const UserInfo& user_info : data.user_info_list()) {
    os << user_info << ", ";
  }
  os << "], and promo code info list: [";
  for (const PromoCodeInfo& promo_code_info : data.promo_code_info_list()) {
    os << promo_code_info << ", ";
  }
  os << "], footer commands: [";
  for (const FooterCommand& footer_command : data.footer_commands()) {
    os << footer_command << ", ";
  }
  return os << "]";
}

AccessorySheetData::Builder::Builder(AccessoryTabType type,
                                     std::u16string title)
    : accessory_sheet_data_(type, std::move(title)) {}

AccessorySheetData::Builder::~Builder() = default;

AccessorySheetData::Builder&& AccessorySheetData::Builder::SetWarning(
    std::u16string warning) && {
  // Calls SetWarning(std::u16string warning)()& since |this| is an lvalue.
  return std::move(SetWarning(std::move(warning)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::SetWarning(
    std::u16string warning) & {
  accessory_sheet_data_.set_warning(std::move(warning));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::SetOptionToggle(
    std::u16string display_text,
    bool enabled,
    AccessoryAction action) && {
  // Calls SetOptionToggle(...)& since |this| is an lvalue.
  return std::move(SetOptionToggle(std::move(display_text), enabled, action));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::SetOptionToggle(
    std::u16string display_text,
    bool enabled,
    AccessoryAction action) & {
  accessory_sheet_data_.set_option_toggle(
      OptionToggle(std::move(display_text), enabled, action));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AddUserInfo(
    std::string origin,
    UserInfo::IsExactMatch is_exact_match,
    GURL icon_url) && {
  // Calls AddUserInfo()& since |this| is an lvalue.
  return std::move(
      AddUserInfo(std::move(origin), is_exact_match, std::move(icon_url)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AddUserInfo(
    std::string origin,
    UserInfo::IsExactMatch is_exact_match,
    GURL icon_url) & {
  accessory_sheet_data_.add_user_info(
      UserInfo(std::move(origin), is_exact_match, std::move(icon_url)));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AppendSimpleField(
    std::u16string text) && {
  // Calls AppendSimpleField(...)& since |this| is an lvalue.
  return std::move(AppendSimpleField(std::move(text)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AppendSimpleField(
    std::u16string text) & {
  std::u16string display_text = text;
  std::u16string text_to_fill = text;
  std::u16string a11y_description = std::move(text);
  return AppendField(std::move(display_text), std::move(text_to_fill),
                     std::move(a11y_description), false, true);
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AppendField(
    std::u16string display_text,
    std::u16string a11y_description,
    bool is_obfuscated,
    bool selectable) && {
  std::u16string text_to_fill = display_text;
  // Calls AppendField(...)& since |this| is an lvalue.
  return std::move(AppendField(std::move(display_text), std::move(text_to_fill),
                               std::move(a11y_description), is_obfuscated,
                               selectable));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AppendField(
    std::u16string display_text,
    std::u16string text_to_fill,
    std::u16string a11y_description,
    bool is_obfuscated,
    bool selectable) & {
  accessory_sheet_data_.mutable_user_info_list().back().add_field(
      AccessorySheetField(std::move(display_text), std::move(text_to_fill),
                          std::move(a11y_description), /*id=*/std::string(),
                          is_obfuscated, selectable));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AppendField(
    std::u16string display_text,
    std::u16string text_to_fill,
    std::u16string a11y_description,
    std::string id,
    bool is_obfuscated,
    bool selectable) && {
  // Calls AppendField(...)& since |this| is an lvalue.
  return std::move(AppendField(std::move(display_text), std::move(text_to_fill),
                               std::move(a11y_description), std::move(id),
                               is_obfuscated, selectable));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AppendField(
    std::u16string display_text,
    std::u16string text_to_fill,
    std::u16string a11y_description,
    std::string id,
    bool is_obfuscated,
    bool selectable) & {
  accessory_sheet_data_.mutable_user_info_list().back().add_field(
      AccessorySheetField(std::move(display_text), std::move(text_to_fill),
                          std::move(a11y_description), std::move(id),
                          is_obfuscated, selectable));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AddPromoCodeInfo(
    std::u16string promo_code,
    std::u16string details_text) && {
  // Calls PromoCodeInfo(...)& since |this| is an lvalue.
  return std::move(
      AddPromoCodeInfo(std::move(promo_code), std::move(details_text)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AddPromoCodeInfo(
    std::u16string promo_code,
    std::u16string details_text) & {
  accessory_sheet_data_.add_promo_code_info(
      (PromoCodeInfo(std::move(promo_code), std::move(details_text))));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AppendFooterCommand(
    std::u16string display_text,
    AccessoryAction action) && {
  // Calls AppendFooterCommand(...)& since |this| is an lvalue.
  return std::move(AppendFooterCommand(std::move(display_text), action));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AppendFooterCommand(
    std::u16string display_text,
    AccessoryAction action) & {
  accessory_sheet_data_.add_footer_command(
      FooterCommand(std::move(display_text), action));
  return *this;
}

AccessorySheetData&& AccessorySheetData::Builder::Build() && {
  return std::move(accessory_sheet_data_);
}

}  // namespace autofill
