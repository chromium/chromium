// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/icubridge/list_formatter.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace critical_actions {

namespace {

constexpr char kFormFillTypesKey[] = "form_fill_types";

// Strings stored in `metadata` for each FormFillType. Never change an existing
// string.
constexpr auto kFormFillTypeStrings =
    base::MakeFixedFlatMap<FormFillType, std::string_view>({
        {FormFillType::kAddress, "ADDRESS"},
        {FormFillType::kName, "NAME"},
        {FormFillType::kEmail, "EMAIL"},
        {FormFillType::kPhone, "PHONE"},
        {FormFillType::kCreditCard, "CREDIT_CARD"},
    });

// Every `FormFillType` in `FormFillTypes` (all but `kUnknown`) needs a string.
static_assert(kFormFillTypeStrings.size() == FormFillTypes::kValueCount);
static_assert(!kFormFillTypeStrings.contains(FormFillType::kUnknown));

// Returns the FormFillType for a value read from `metadata`, or std::nullopt
// if `value` isn't a string or isn't a known FormFillType string.
std::optional<FormFillType> FormFillTypeFromString(const base::Value& value) {
  const std::string* type_string = value.GetIfString();
  if (!type_string) {
    return std::nullopt;
  }
  for (const auto& [type, string] : kFormFillTypeStrings) {
    if (string == *type_string) {
      return type;
    }
  }
  return std::nullopt;
}

// Returns the FormFillTypes from the "form_fill_types" list in `metadata_dict`.
// Returns an empty set if the list is missing, skips values that aren't known
// FormFillType strings.
FormFillTypes ParseFormFillTypes(const base::DictValue& metadata_dict) {
  const base::ListValue* form_fill_types =
      metadata_dict.FindList(kFormFillTypesKey);
  if (!form_fill_types) {
    return {};
  }
  FormFillTypes types;
  for (const base::Value& value : *form_fill_types) {
    if (std::optional<FormFillType> type = FormFillTypeFromString(value)) {
      types.Put(*type);
    }
  }
  return types;
}

// Returns the localized name of one filled contact info attribute, e.g.
// "Address".
std::u16string GetContactInfoTypeName(FormFillType type) {
  switch (type) {
    case FormFillType::kAddress:
      return l10n_util::GetStringUTF16(
          IDS_HISTORY_CRITICAL_ACTION_FORM_FILL_TYPE_ADDRESS);
    case FormFillType::kName:
      return l10n_util::GetStringUTF16(
          IDS_HISTORY_CRITICAL_ACTION_FORM_FILL_TYPE_NAME);
    case FormFillType::kEmail:
      return l10n_util::GetStringUTF16(
          IDS_HISTORY_CRITICAL_ACTION_FORM_FILL_TYPE_EMAIL);
    case FormFillType::kPhone:
      return l10n_util::GetStringUTF16(
          IDS_HISTORY_CRITICAL_ACTION_FORM_FILL_TYPE_PHONE);
    case FormFillType::kCreditCard:
    case FormFillType::kUnknown:
      // Only contact info attributes are listed, and `FormFillTypes` never
      // contains `kUnknown`.
      NOTREACHED();
  }
}

// Joins `names` into a localized list, e.g. "Address, Name, Email" in
// English.
std::u16string JoinFormFillTypeNames(const std::vector<std::u16string>& names) {
  return base::i18n::IcuBridge::GetInstance()
      .list_formatter()
      ->FormatNarrowList(names);
}

// Returns the label of a contact info form fill with `types`, e.g. "Contact
// info filled: Address, Name", or "Contact info filled" if it has no contact
// info attributes.
std::string GetContactInfoLabel(FormFillTypes types) {
  std::vector<std::u16string> names;
  for (FormFillType type : types) {
    // Skip attributes of another group, which don't belong under this
    // action's label, tooltip and linkout.
    if (GetFormFillGroup(type) == ActionType::kFormFillContactInfo) {
      names.push_back(GetContactInfoTypeName(type));
    }
  }
  if (names.empty()) {
    return l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_FILLED);
  }
  return l10n_util::GetStringFUTF8(
      IDS_HISTORY_CRITICAL_ACTION_CONTACT_INFO_FILLED_WITH_TYPES,
      JoinFormFillTypeNames(names));
}

}  // namespace

bool IsGranularFormFill(ActionType type) {
  switch (type) {
    case ActionType::kFormFillContactInfo:
    case ActionType::kFormFillPayments:
      return true;
    case ActionType::kUnknown:
    case ActionType::kFormFill:
    case ActionType::kDownload:
    case ActionType::kSettingChange:
    case ActionType::kCredentialAccess:
    case ActionType::kGooglePasswordManager:
    case ActionType::kFederatedLogin:
    case ActionType::kCredentialsOtp:
    case ActionType::kWebMcpTool:
      return false;
  }
}

ActionType GetFormFillGroup(FormFillType type) {
  switch (type) {
    case FormFillType::kAddress:
    case FormFillType::kName:
    case FormFillType::kEmail:
    case FormFillType::kPhone:
      return ActionType::kFormFillContactInfo;
    case FormFillType::kCreditCard:
      return ActionType::kFormFillPayments;
    case FormFillType::kUnknown:
      // `FormFillType::kUnknown` is never parsed from metadata or produced
      // when logging.
      NOTREACHED();
  }
}

void SetFormFillTypes(base::DictValue& metadata_dict, FormFillTypes types) {
  base::ListValue form_fill_types;
  for (FormFillType type : types) {
    form_fill_types.Append(kFormFillTypeStrings.at(type));
  }
  metadata_dict.Set(kFormFillTypesKey, std::move(form_fill_types));
}

CriticalActionEntry::CriticalActionEntry() = default;

CriticalActionEntry::CriticalActionEntry(const CriticalActionEntry&) = default;

CriticalActionEntry::CriticalActionEntry(CriticalActionEntry&&) noexcept =
    default;

CriticalActionEntry& CriticalActionEntry::operator=(
    const CriticalActionEntry&) = default;

CriticalActionEntry& CriticalActionEntry::operator=(
    CriticalActionEntry&&) noexcept = default;

CriticalActionEntry::~CriticalActionEntry() = default;

std::string CriticalActionEntry::GetLabel() const {
  switch (action_type) {
    case ActionType::kCredentialAccess:
    case ActionType::kGooglePasswordManager:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_PASSWORD_FILLED);
    case ActionType::kFederatedLogin:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_FEDERATED_LOGIN);
    case ActionType::kCredentialsOtp:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_CREDENTIALS_OTP);
    case ActionType::kFormFill:
      return l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_FILLED);
    case ActionType::kFormFillContactInfo:
      return GetContactInfoLabel(GetFormFillTypes());
    case ActionType::kFormFillPayments:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_CARD_DETAILS_FILLED);
    case ActionType::kDownload:
      return l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_DOWNLOAD);
    case ActionType::kSettingChange:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_SETTING_CHANGE);
    case ActionType::kWebMcpTool: {
      if (!metadata.empty()) {
        std::optional<base::DictValue> parsed =
            base::JSONReader::ReadDict(metadata, base::JSON_PARSE_RFC);
        if (parsed) {
          if (const std::string* title = parsed->FindString("tool_title");
              title && !title->empty()) {
            return *title;
          }
          if (const std::string* name = parsed->FindString("tool_name");
              name && !name->empty()) {
            return l10n_util::GetStringFUTF8(
                IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL_WITH_NAME,
                base::UTF8ToUTF16(*name));
          }
        }
      }
      return l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL);
    }
    case ActionType::kUnknown:
      return "";
  }
}

std::string CriticalActionEntry::GetTooltip() const {
  switch (action_type) {
    case ActionType::kCredentialAccess:
    case ActionType::kGooglePasswordManager:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_PASSWORD_TOOLTIP);
    case ActionType::kFederatedLogin:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_FEDERATED_LOGIN_TOOLTIP);
    case ActionType::kCredentialsOtp:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_CREDENTIALS_OTP_TOOLTIP);
    case ActionType::kFormFill:
    case ActionType::kFormFillContactInfo:
      return l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_TOOLTIP);
    case ActionType::kFormFillPayments:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_PAYMENTS_TOOLTIP);
    case ActionType::kDownload:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_DOWNLOAD_TOOLTIP);
    case ActionType::kSettingChange:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_SETTING_TOOLTIP);
    case ActionType::kWebMcpTool:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL_TOOLTIP);
    case ActionType::kUnknown:
      return "";
  }
}

FormFillTypes CriticalActionEntry::GetFormFillTypes() const {
  std::optional<base::DictValue> metadata_dict =
      base::JSONReader::ReadDict(metadata, base::JSON_PARSE_RFC);
  return metadata_dict ? ParseFormFillTypes(*metadata_dict) : FormFillTypes();
}

void CriticalActionEntry::AddFormFillTypes(FormFillTypes types) {
  if (types.empty()) {
    return;
  }
  base::DictValue metadata_dict =
      base::JSONReader::ReadDict(metadata, base::JSON_PARSE_RFC)
          .value_or(base::DictValue());
  FormFillTypes merged = ParseFormFillTypes(metadata_dict);
  merged.PutAll(types);
  SetFormFillTypes(metadata_dict, merged);
  metadata = base::WriteJson(metadata_dict).value_or(metadata);
}

CriticalActionQueryOptions::CriticalActionQueryOptions() = default;

CriticalActionQueryOptions::CriticalActionQueryOptions(
    const CriticalActionQueryOptions&) = default;

CriticalActionQueryOptions::CriticalActionQueryOptions(
    CriticalActionQueryOptions&&) noexcept = default;

CriticalActionQueryOptions& CriticalActionQueryOptions::operator=(
    const CriticalActionQueryOptions&) = default;

CriticalActionQueryOptions& CriticalActionQueryOptions::operator=(
    CriticalActionQueryOptions&&) noexcept = default;

CriticalActionQueryOptions::~CriticalActionQueryOptions() = default;

CriticalActionEntry::Builder::Builder() {
  entry_.critical_action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry_.timestamp = base::Time::Now();
}

CriticalActionEntry::Builder::~Builder() = default;

CriticalActionEntry::Builder::Builder(Builder&&) noexcept = default;
CriticalActionEntry::Builder& CriticalActionEntry::Builder::operator=(
    Builder&&) noexcept = default;

CriticalActionEntry::Builder&&
CriticalActionEntry::Builder::SetCriticalActionId(
    std::string critical_action_id_val) && {
  entry_.critical_action_id = std::move(critical_action_id_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetTimestamp(
    base::Time timestamp_val) && {
  entry_.timestamp = timestamp_val;
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetActionType(
    ActionType action_type_val) && {
  entry_.action_type = action_type_val;
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetActionSource(
    ActionSource action_source_val) && {
  entry_.action_source = action_source_val;
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetConversationId(
    std::string conversation_id_val) && {
  entry_.conversation_id = std::move(conversation_id_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&&
CriticalActionEntry::Builder::SetConversationAnchorId(
    std::string conversation_anchor_id_val) && {
  entry_.conversation_anchor_id = std::move(conversation_anchor_id_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetActorTaskId(
    std::string actor_task_id_val) && {
  entry_.actor_task_id = std::move(actor_task_id_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetMetadata(
    std::string metadata_val) && {
  entry_.metadata = std::move(metadata_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetVisitId(
    int64_t visit_id_val) && {
  entry_.visit_id = visit_id_val;
  return std::move(*this);
}

CriticalActionEntry CriticalActionEntry::Builder::Build() && {
  CHECK(!entry_.critical_action_id.empty());
  CHECK(!entry_.timestamp.is_null());
  CHECK(entry_.action_type != ActionType::kUnknown);
  CHECK(entry_.action_source != ActionSource::kUnknown);

  return std::move(entry_);
}

}  // namespace critical_actions
