// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ACCESSORY_SHEET_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ACCESSORY_SHEET_DATA_H_

#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"

namespace autofill {

// Represents user data to be shown on the manual fallback UI (e.g. a Profile,
// or a Credit Card, or the credentials for a website).
class UserInfo {
 public:
  // Represents a selectable item, such as the username or a credit card
  // number.
  class Field {
   public:
    Field(base::string16 display_text,
          base::string16 a11y_description,
          bool is_obfuscated,
          bool selectable);
    Field(base::string16 display_text,
          base::string16 a11y_description,
          std::string id,
          bool is_obfuscated,
          bool selectable);
    Field(const Field& field);
    Field(Field&& field);

    ~Field();

    Field& operator=(const Field& field);
    Field& operator=(Field&& field);

    const base::string16& display_text() const { return display_text_; }

    const base::string16& a11y_description() const { return a11y_description_; }

    const std::string& id() const { return id_; }

    bool is_obfuscated() const { return is_obfuscated_; }

    bool selectable() const { return selectable_; }

    bool operator==(const UserInfo::Field& field) const;

   private:
    base::string16 display_text_;
    base::string16 a11y_description_;
    std::string id_;  // Optional, if needed to complete filling.
    bool is_obfuscated_;
    bool selectable_;
  };

  UserInfo();
  explicit UserInfo(std::string origin);
  UserInfo(const UserInfo& user_info);
  UserInfo(UserInfo&& field);

  ~UserInfo();

  UserInfo& operator=(const UserInfo& user_info);
  UserInfo& operator=(UserInfo&& user_info);

  void add_field(Field field) { fields_.push_back(std::move(field)); }

  const std::vector<Field>& fields() const { return fields_; }
  const std::string& origin() const { return origin_; }

  bool operator==(const UserInfo& user_info) const;

 private:
  std::string origin_;
  std::vector<Field> fields_;
};

std::ostream& operator<<(std::ostream& out, const UserInfo::Field& field);
std::ostream& operator<<(std::ostream& out, const UserInfo& user_info);

// Represents a command below the suggestions, such as "Manage password...".
class FooterCommand {
 public:
  FooterCommand(base::string16 display_text, autofill::AccessoryAction action);
  FooterCommand(const FooterCommand& footer_command);
  FooterCommand(FooterCommand&& footer_command);

  ~FooterCommand();

  FooterCommand& operator=(const FooterCommand& footer_command);
  FooterCommand& operator=(FooterCommand&& footer_command);

  const base::string16& display_text() const { return display_text_; }

  autofill::AccessoryAction accessory_action() const {
    return accessory_action_;
  }

  bool operator==(const FooterCommand& fc) const;

 private:
  base::string16 display_text_;
  autofill::AccessoryAction accessory_action_;
};

std::ostream& operator<<(std::ostream& out, const FooterCommand& fc);

std::ostream& operator<<(std::ostream& out, const AccessoryTabType& type);

// Represents the contents of a bottom sheet tab below the keyboard accessory,
// which can correspond to passwords, credit cards, or profiles data.
class AccessorySheetData {
 public:
  class Builder;

  AccessorySheetData(AccessoryTabType sheet_type, base::string16 title);
  AccessorySheetData(AccessoryTabType sheet_type,
                     base::string16 title,
                     base::string16 warning);
  AccessorySheetData(const AccessorySheetData& data);
  AccessorySheetData(AccessorySheetData&& data);

  ~AccessorySheetData();

  AccessorySheetData& operator=(const AccessorySheetData& data);
  AccessorySheetData& operator=(AccessorySheetData&& data);

  const base::string16& title() const { return title_; }
  AccessoryTabType get_sheet_type() const { return sheet_type_; }

  const base::string16& warning() const { return warning_; }
  void set_warning(base::string16 warning) { warning_ = std::move(warning); }

  void add_user_info(UserInfo user_info) {
    user_info_list_.emplace_back(std::move(user_info));
  }

  const std::vector<UserInfo>& user_info_list() const {
    return user_info_list_;
  }

  std::vector<UserInfo>& mutable_user_info_list() { return user_info_list_; }

  void add_footer_command(FooterCommand footer_command) {
    footer_commands_.emplace_back(std::move(footer_command));
  }

  const std::vector<FooterCommand>& footer_commands() const {
    return footer_commands_;
  }

  bool operator==(const AccessorySheetData& data) const;

 private:
  AccessoryTabType sheet_type_;
  base::string16 title_;
  base::string16 warning_;
  std::vector<UserInfo> user_info_list_;
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
  Builder(AccessoryTabType type, base::string16 title);
  ~Builder();

  // Adds a warning string to the accessory sheet.
  Builder&& SetWarning(base::string16 warning) &&;
  Builder& SetWarning(base::string16 warning) &;

  // Adds a new UserInfo object to |accessory_sheet_data_|.
  Builder&& AddUserInfo(std::string origin = std::string()) &&;
  Builder& AddUserInfo(std::string origin = std::string()) &;

  // Appends a selectable, non-obfuscated field to the last UserInfo object.
  Builder&& AppendSimpleField(base::string16 text) &&;
  Builder& AppendSimpleField(base::string16 text) &;

  // Appends a field to the last UserInfo object.
  Builder&& AppendField(base::string16 display_text,
                        base::string16 a11y_description,
                        bool is_obfuscated,
                        bool selectable) &&;
  Builder& AppendField(base::string16 display_text,
                       base::string16 a11y_description,
                       bool is_obfuscated,
                       bool selectable) &;

  Builder&& AppendField(base::string16 display_text,
                        base::string16 a11y_description,
                        std::string id,
                        bool is_obfuscated,
                        bool selectable) &&;
  Builder& AppendField(base::string16 display_text,
                       base::string16 a11y_description,
                       std::string id,
                       bool is_obfuscated,
                       bool selectable) &;

  // Appends a new footer command to |accessory_sheet_data_|.
  Builder&& AppendFooterCommand(base::string16 display_text,
                                autofill::AccessoryAction action) &&;
  Builder& AppendFooterCommand(base::string16 display_text,
                               autofill::AccessoryAction action) &;

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
