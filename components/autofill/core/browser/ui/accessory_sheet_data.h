// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ACCESSORY_SHEET_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ACCESSORY_SHEET_DATA_H_

#include <string>
#include <utility>
#include <vector>

#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace autofill {

// Represents a selectable item within a UserInfo or a PromoCodeInfo in the
// manual fallback UI, such as the username or a credit card number or a promo
// code.
class AccessorySheetField {
 public:
  AccessorySheetField(std::u16string display_text,
                      std::u16string text_to_fill,
                      std::u16string a11y_description,
                      std::string id,
                      bool is_obfuscated,
                      bool selectable);
  AccessorySheetField(const AccessorySheetField& field);
  AccessorySheetField(AccessorySheetField&& field);

  ~AccessorySheetField();

  AccessorySheetField& operator=(const AccessorySheetField& field);
  AccessorySheetField& operator=(AccessorySheetField&& field);

  const std::u16string& display_text() const { return display_text_; }

  const std::u16string& text_to_fill() const { return text_to_fill_; }

  const std::u16string& a11y_description() const { return a11y_description_; }

  const std::string& id() const { return id_; }

  bool is_obfuscated() const { return is_obfuscated_; }

  bool selectable() const { return selectable_; }

  bool operator==(const AccessorySheetField& field) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  // IMPORTANT(https://crbug.com/1169167): Add the size of newly added strings
  // to the memory estimation member!
  std::u16string display_text_;
  // The string that would be used to fill in the form, for cases when it is
  // different from |display_text_|. For example: For unmasked credit cards,
  // the `display_text` contains spaces where as the `text_to_fill_` would
  // contain the card number without any spaces.
  std::u16string text_to_fill_;
  std::u16string a11y_description_;
  std::string id_;  // Optional, if needed to complete filling.
  bool is_obfuscated_;
  bool selectable_;
  size_t estimated_memory_use_by_strings_ = 0;
};

// Represents user data to be shown on the manual fallback UI (e.g. a Profile,
// or a Credit Card, or the credentials for a website). For credentials,
// 'is_exact_match' is used to determine the origin (first-party match, a PSL or
// affiliated match) of the credential.
class UserInfo {
 public:
  using IsExactMatch = base::StrongAlias<class IsExactMatchTag, bool>;

  UserInfo();
  explicit UserInfo(std::string origin);
  UserInfo(std::string origin, IsExactMatch is_exact_match);
  UserInfo(std::string origin, GURL icon_url);
  UserInfo(std::string origin, IsExactMatch is_exact_match, GURL icon_url);
  UserInfo(const UserInfo& user_info);
  UserInfo(UserInfo&& field);

  ~UserInfo();

  UserInfo& operator=(const UserInfo& user_info);
  UserInfo& operator=(UserInfo&& user_info);

  void add_field(AccessorySheetField field) {
    estimated_dynamic_memory_use_ += field.EstimateMemoryUsage();
    fields_.push_back(std::move(field));
  }

  const std::vector<AccessorySheetField>& fields() const { return fields_; }
  const std::string& origin() const { return origin_; }
  IsExactMatch is_exact_match() const { return is_exact_match_; }
  const GURL icon_url() const { return icon_url_; }

  bool operator==(const UserInfo& user_info) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  // IMPORTANT(https://crbug.com/1169167): Add the size of newly added strings
  // to the memory estimation member!
  std::string origin_;
  // True means it's neither PSL match nor affiliated match, false otherwise.
  IsExactMatch is_exact_match_{true};
  std::vector<AccessorySheetField> fields_;
  GURL icon_url_;
  size_t estimated_dynamic_memory_use_ = 0;
};

std::ostream& operator<<(std::ostream& out, const AccessorySheetField& field);
std::ostream& operator<<(std::ostream& out, const UserInfo& user_info);

// Represents a passkey entry shown in the password accessory.
class PasskeySection {
 public:
  PasskeySection(std::string display_name, std::vector<uint8_t> passkey_id);
  PasskeySection(const PasskeySection& passkey_section);
  PasskeySection(PasskeySection&& passkey_section);

  ~PasskeySection();

  PasskeySection& operator=(const PasskeySection& passkey_section);
  PasskeySection& operator=(PasskeySection&& passkey_section);

  const std::string display_name() const { return display_name_; }

  const std::vector<uint8_t> passkey_id() const { return passkey_id_; }

  bool operator==(const PasskeySection& passkey_section) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  std::string display_name_;
  std::vector<uint8_t> passkey_id_;
  size_t estimated_dynamic_memory_use_ = 0;
};

std::ostream& operator<<(std::ostream& out,
                         const PasskeySection& passkey_section);

// Represents data pertaining to promo code offers to be shown on the Payments
// tab of manual fallback UI.
class PromoCodeInfo {
 public:
  PromoCodeInfo(std::u16string promo_code, std::u16string details_text);
  PromoCodeInfo(const PromoCodeInfo& promo_code_info);
  PromoCodeInfo(PromoCodeInfo&& promo_code_info);

  ~PromoCodeInfo();

  PromoCodeInfo& operator=(const PromoCodeInfo& promo_code_info);
  PromoCodeInfo& operator=(PromoCodeInfo&& promo_code_info);

  const AccessorySheetField promo_code() const { return promo_code_; }

  const std::u16string details_text() const { return details_text_; }

  bool operator==(const PromoCodeInfo& promo_code_info) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  AccessorySheetField promo_code_;
  std::u16string details_text_;
  size_t estimated_dynamic_memory_use_ = 0;
};

std::ostream& operator<<(std::ostream& out,
                         const PromoCodeInfo& promo_code_info);

// Represents a command below the suggestions, such as "Manage password...".
class FooterCommand {
 public:
  FooterCommand(std::u16string display_text, AccessoryAction action);
  FooterCommand(const FooterCommand& footer_command);
  FooterCommand(FooterCommand&& footer_command);

  ~FooterCommand();

  FooterCommand& operator=(const FooterCommand& footer_command);
  FooterCommand& operator=(FooterCommand&& footer_command);

  const std::u16string& display_text() const { return display_text_; }

  AccessoryAction accessory_action() const { return accessory_action_; }

  bool operator==(const FooterCommand& fc) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  // IMPORTANT(https://crbug.com/1169167): Add the size of newly added strings
  // to the memory estimation member!
  std::u16string display_text_;
  AccessoryAction accessory_action_;
  size_t estimated_memory_use_by_strings_ = 0;
};

std::ostream& operator<<(std::ostream& out, const FooterCommand& fc);

std::ostream& operator<<(std::ostream& out, const AccessoryTabType& type);

// Toggle to be displayed above the suggestions. One such toggle can be used,
// for example, to turn password saving on for the current origin.
class OptionToggle {
 public:
  OptionToggle(std::u16string display_text,
               bool enabled,
               AccessoryAction accessory_action);
  OptionToggle(const OptionToggle& option_toggle);
  OptionToggle(OptionToggle&& option_toggle);

  ~OptionToggle();

  OptionToggle& operator=(const OptionToggle& option_toggle);
  OptionToggle& operator=(OptionToggle&& option_toggle);

  const std::u16string& display_text() const { return display_text_; }

  bool is_enabled() const { return enabled_; }

  AccessoryAction accessory_action() const { return accessory_action_; }

  bool operator==(const OptionToggle& option_toggle) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  // IMPORTANT(https://crbug.com/1169167): Add the size of newly added strings
  // to the memory estimation member!
  std::u16string display_text_;
  bool enabled_;
  AccessoryAction accessory_action_;
  size_t estimated_memory_use_by_strings_ = 0;
};

// Represents the contents of a bottom sheet tab below the keyboard accessory,
// which can correspond to passwords, credit cards, or profiles data.
class AccessorySheetData {
 public:
  class Builder;

  AccessorySheetData(AccessoryTabType sheet_type, std::u16string title);
  AccessorySheetData(AccessoryTabType sheet_type,
                     std::u16string title,
                     std::u16string warning);
  AccessorySheetData(const AccessorySheetData& data);
  AccessorySheetData(AccessorySheetData&& data);

  ~AccessorySheetData();

  AccessorySheetData& operator=(const AccessorySheetData& data);
  AccessorySheetData& operator=(AccessorySheetData&& data);

  const std::u16string& title() const { return title_; }
  AccessoryTabType get_sheet_type() const { return sheet_type_; }

  const std::u16string& warning() const { return warning_; }
  void set_warning(std::u16string warning) { warning_ = std::move(warning); }

  void set_option_toggle(OptionToggle toggle) {
    option_toggle_ = std::move(toggle);
  }
  const absl::optional<OptionToggle>& option_toggle() const {
    return option_toggle_;
  }

  void add_user_info(UserInfo user_info) {
    user_info_list_.emplace_back(std::move(user_info));
  }

  void add_passkey_section(PasskeySection passkey_section) {
    passkey_section_list_.emplace_back(std::move(passkey_section));
  }

  const std::vector<UserInfo>& user_info_list() const {
    return user_info_list_;
  }

  const std::vector<PasskeySection>& passkey_section_list() const {
    return passkey_section_list_;
  }

  std::vector<UserInfo>& mutable_user_info_list() { return user_info_list_; }

  void add_promo_code_info(PromoCodeInfo promo_code_info) {
    promo_code_info_list_.emplace_back(std::move(promo_code_info));
  }

  const std::vector<PromoCodeInfo>& promo_code_info_list() const {
    return promo_code_info_list_;
  }

  void add_footer_command(FooterCommand footer_command) {
    footer_commands_.emplace_back(std::move(footer_command));
  }

  const std::vector<FooterCommand>& footer_commands() const {
    return footer_commands_;
  }

  bool operator==(const AccessorySheetData& data) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  AccessoryTabType sheet_type_;
  std::u16string title_;
  std::u16string warning_;
  absl::optional<OptionToggle> option_toggle_;
  std::vector<PasskeySection> passkey_section_list_;
  std::vector<UserInfo> user_info_list_;
  std::vector<PromoCodeInfo> promo_code_info_list_;
  std::vector<FooterCommand> footer_commands_;
};

std::ostream& operator<<(std::ostream& out, const AccessorySheetData& data);

// Helper class for AccessorySheetData objects creation.
//
// Example that creates a AccessorySheetData object with two UserInfo objects;
// the former has two fields, whereas the latter has three fields:
//   AccessorySheetData data = AccessorySheetData::Builder(title)
//       .AddUserInfo()
//           .AppendField(...)
//           .AppendField(...)
//       .AddUserInfo()
//           .AppendField(...)
//           .AppendField(...)
//           .AppendField(...)
//       .Build();
class AccessorySheetData::Builder {
 public:
  Builder(AccessoryTabType type, std::u16string title);
  ~Builder();

  // Adds a warning string to the accessory sheet.
  Builder&& SetWarning(std::u16string warning) &&;
  Builder& SetWarning(std::u16string warning) &;

  // Sets the option toggle in the accessory sheet.
  Builder&& SetOptionToggle(std::u16string display_text,
                            bool enabled,
                            AccessoryAction action) &&;
  Builder& SetOptionToggle(std::u16string display_text,
                           bool enabled,
                           AccessoryAction action) &;

  // Adds a new UserInfo object to |accessory_sheet_data_|.
  Builder&& AddUserInfo(
      std::string origin = std::string(),
      UserInfo::IsExactMatch is_exact_match = UserInfo::IsExactMatch(true),
      GURL icon_url = GURL()) &&;
  Builder& AddUserInfo(
      std::string origin = std::string(),
      UserInfo::IsExactMatch is_exact_match = UserInfo::IsExactMatch(true),
      GURL icon_url = GURL()) &;

  // Appends a selectable, non-obfuscated field to the last UserInfo object.
  Builder&& AppendSimpleField(std::u16string text) &&;
  Builder& AppendSimpleField(std::u16string text) &;

  // Appends a field to the last UserInfo object.
  Builder&& AppendField(std::u16string display_text,
                        std::u16string a11y_description,
                        bool is_obfuscated,
                        bool selectable) &&;
  Builder& AppendField(std::u16string display_text,
                       std::u16string text_to_fill,
                       std::u16string a11y_description,
                       bool is_obfuscated,
                       bool selectable) &;

  Builder&& AppendField(std::u16string display_text,
                        std::u16string text_to_fill,
                        std::u16string a11y_description,
                        std::string id,
                        bool is_obfuscated,
                        bool selectable) &&;
  Builder& AppendField(std::u16string display_text,
                       std::u16string text_to_fill,
                       std::u16string a11y_description,
                       std::string id,
                       bool is_obfuscated,
                       bool selectable) &;

  // Adds a new PasskeySection `accessory_sheet_data_`.
  Builder&& AddPasskeySection(std::string username,
                              std::vector<uint8_t> credential_id) &&;
  Builder& AddPasskeySection(std::string username,
                             std::vector<uint8_t> credential_id) &;

  // Adds a new PromoCodeInfo object to |accessory_sheet_data_|.
  Builder&& AddPromoCodeInfo(std::u16string promo_code,
                             std::u16string details_text) &&;
  Builder& AddPromoCodeInfo(std::u16string promo_code,
                            std::u16string details_text) &;

  // Appends a new footer command to |accessory_sheet_data_|.
  Builder&& AppendFooterCommand(std::u16string display_text,
                                AccessoryAction action) &&;
  Builder& AppendFooterCommand(std::u16string display_text,
                               AccessoryAction action) &;

  // This class returns the constructed AccessorySheetData object. Since this
  // would render the builder unusable, it's required to destroy the object
  // afterwards. So if you hold the class in a variable, invoke like this:
  //   AccessorySheetData::Builder b(title);
  //   std::move(b).Build();
  AccessorySheetData&& Build() &&;

 private:
  AccessorySheetData accessory_sheet_data_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ACCESSORY_SHEET_DATA_H_
