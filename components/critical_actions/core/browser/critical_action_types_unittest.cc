// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_types.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace critical_actions {
namespace {

TEST(CriticalActionTypesTest, LabelsAndTooltipsForActionTypes) {
  CriticalActionEntry credential_action;
  credential_action.action_type = ActionType::kCredentialAccess;
  EXPECT_EQ(
      credential_action.GetLabel(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_PASSWORD_FILLED));
  EXPECT_EQ(
      credential_action.GetTooltip(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_PASSWORD_TOOLTIP));

  CriticalActionEntry form_action;
  form_action.action_type = ActionType::kFormFill;
  EXPECT_EQ(form_action.GetLabel(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_FILLED));
  EXPECT_EQ(form_action.GetTooltip(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_TOOLTIP));

  CriticalActionEntry contact_info_action;
  contact_info_action.action_type = ActionType::kFormFillContactInfo;
  EXPECT_EQ(contact_info_action.GetLabel(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_FILLED));
  EXPECT_EQ(contact_info_action.GetTooltip(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_TOOLTIP));

  CriticalActionEntry payments_action;
  payments_action.action_type = ActionType::kFormFillPayments;
  EXPECT_EQ(payments_action.GetLabel(),
            l10n_util::GetStringUTF8(
                IDS_HISTORY_CRITICAL_ACTION_CARD_DETAILS_FILLED));
  EXPECT_EQ(
      payments_action.GetTooltip(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_PAYMENTS_TOOLTIP));

  CriticalActionEntry download_action;
  download_action.action_type = ActionType::kDownload;
  EXPECT_EQ(download_action.GetLabel(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_DOWNLOAD));
  EXPECT_EQ(
      download_action.GetTooltip(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_DOWNLOAD_TOOLTIP));

  CriticalActionEntry setting_action;
  setting_action.action_type = ActionType::kSettingChange;
  EXPECT_EQ(
      setting_action.GetLabel(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_SETTING_CHANGE));
  EXPECT_EQ(
      setting_action.GetTooltip(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_SETTING_TOOLTIP));

  CriticalActionEntry gpm_action;
  gpm_action.action_type = ActionType::kGooglePasswordManager;
  EXPECT_EQ(
      gpm_action.GetLabel(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_PASSWORD_FILLED));
  EXPECT_EQ(
      gpm_action.GetTooltip(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_PASSWORD_TOOLTIP));

  CriticalActionEntry federated_action;
  federated_action.action_type = ActionType::kFederatedLogin;
  EXPECT_EQ(
      federated_action.GetLabel(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FEDERATED_LOGIN));
  EXPECT_EQ(federated_action.GetTooltip(),
            l10n_util::GetStringUTF8(
                IDS_HISTORY_CRITICAL_ACTION_FEDERATED_LOGIN_TOOLTIP));

  CriticalActionEntry otp_action;
  otp_action.action_type = ActionType::kCredentialsOtp;
  EXPECT_EQ(
      otp_action.GetLabel(),
      l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_CREDENTIALS_OTP));
  EXPECT_EQ(otp_action.GetTooltip(),
            l10n_util::GetStringUTF8(
                IDS_HISTORY_CRITICAL_ACTION_CREDENTIALS_OTP_TOOLTIP));

  CriticalActionEntry webmcp_with_title;
  webmcp_with_title.action_type = ActionType::kWebMcpTool;
  webmcp_with_title.metadata =
      R"({"tool_title":"Book Flight to Paris","tool_name":"flight_search"})";
  EXPECT_EQ(webmcp_with_title.GetLabel(), "Book Flight to Paris");
  EXPECT_EQ(webmcp_with_title.GetTooltip(),
            l10n_util::GetStringUTF8(
                IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL_TOOLTIP));

  CriticalActionEntry webmcp_with_name;
  webmcp_with_name.action_type = ActionType::kWebMcpTool;
  webmcp_with_name.metadata = R"({"tool_name":"flight_search"})";
  EXPECT_EQ(
      webmcp_with_name.GetLabel(),
      l10n_util::GetStringFUTF8(
          IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL_WITH_NAME, u"flight_search"));
  EXPECT_EQ(webmcp_with_name.GetTooltip(),
            l10n_util::GetStringUTF8(
                IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL_TOOLTIP));

  CriticalActionEntry webmcp_with_empty_title;
  webmcp_with_empty_title.action_type = ActionType::kWebMcpTool;
  webmcp_with_empty_title.metadata =
      R"({"tool_title":"","tool_name":"flight_search"})";
  EXPECT_EQ(
      webmcp_with_empty_title.GetLabel(),
      l10n_util::GetStringFUTF8(
          IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL_WITH_NAME, u"flight_search"));
  EXPECT_EQ(webmcp_with_empty_title.GetTooltip(),
            l10n_util::GetStringUTF8(
                IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL_TOOLTIP));

  CriticalActionEntry webmcp_without_metadata;
  webmcp_without_metadata.action_type = ActionType::kWebMcpTool;
  EXPECT_EQ(webmcp_without_metadata.GetLabel(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL));
  EXPECT_EQ(webmcp_without_metadata.GetTooltip(),
            l10n_util::GetStringUTF8(
                IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL_TOOLTIP));

  CriticalActionEntry webmcp_with_invalid_json;
  webmcp_with_invalid_json.action_type = ActionType::kWebMcpTool;
  webmcp_with_invalid_json.metadata = "invalid-json";
  EXPECT_EQ(webmcp_with_invalid_json.GetLabel(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL));
  EXPECT_EQ(webmcp_with_invalid_json.GetTooltip(),
            l10n_util::GetStringUTF8(
                IDS_HISTORY_CRITICAL_ACTION_WEBMCP_TOOL_TOOLTIP));

  CriticalActionEntry unknown_action;
  unknown_action.action_type = ActionType::kUnknown;
  EXPECT_EQ(unknown_action.GetLabel(), "");
  EXPECT_EQ(unknown_action.GetTooltip(), "");
}

TEST(CriticalActionTypesTest, BuilderSetsConversationAnchorId) {
  const base::Time timestamp = base::Time::FromTimeT(1700000000);

  CriticalActionEntry with_anchor = CriticalActionEntry::Builder()
                                        .SetCriticalActionId("id_1")
                                        .SetTimestamp(timestamp)
                                        .SetActionType(ActionType::kFormFill)
                                        .SetActionSource(ActionSource::kActor)
                                        .SetConversationId("conv_1")
                                        .SetConversationAnchorId("turn_42")
                                        .Build();
  EXPECT_EQ(with_anchor.conversation_anchor_id, "turn_42");

  CriticalActionEntry without_anchor =
      CriticalActionEntry::Builder()
          .SetCriticalActionId("id_1")
          .SetTimestamp(timestamp)
          .SetActionType(ActionType::kFormFill)
          .SetActionSource(ActionSource::kActor)
          .SetConversationId("conv_1")
          .Build();
  EXPECT_TRUE(without_anchor.conversation_anchor_id.empty());

  // Entries that differ only in `conversation_anchor_id` must not compare
  // equal.
  EXPECT_NE(with_anchor, without_anchor);
}

TEST(CriticalActionTypesTest, IsGranularFormFill) {
  EXPECT_TRUE(IsGranularFormFill(ActionType::kFormFillContactInfo));
  EXPECT_TRUE(IsGranularFormFill(ActionType::kFormFillPayments));
  EXPECT_FALSE(IsGranularFormFill(ActionType::kFormFill));
  EXPECT_FALSE(IsGranularFormFill(ActionType::kDownload));
  EXPECT_FALSE(IsGranularFormFill(ActionType::kUnknown));
}

TEST(CriticalActionTypesTest, FormFillGroups) {
  EXPECT_EQ(GetFormFillGroup(FormFillType::kAddress),
            ActionType::kFormFillContactInfo);
  EXPECT_EQ(GetFormFillGroup(FormFillType::kName),
            ActionType::kFormFillContactInfo);
  EXPECT_EQ(GetFormFillGroup(FormFillType::kEmail),
            ActionType::kFormFillContactInfo);
  EXPECT_EQ(GetFormFillGroup(FormFillType::kPhone),
            ActionType::kFormFillContactInfo);
  EXPECT_EQ(GetFormFillGroup(FormFillType::kCreditCard),
            ActionType::kFormFillPayments);
}

TEST(CriticalActionTypesTest, FormFillTypesRoundTrip) {
  // Every `FormFillType` survives a write and read.
  base::DictValue all_types_dict;
  SetFormFillTypes(all_types_dict, FormFillTypes::All());
  CriticalActionEntry all_types;
  all_types.metadata = base::WriteJson(all_types_dict).value();
  EXPECT_EQ(all_types.GetFormFillTypes(), FormFillTypes::All());

  // The stored strings are stable.
  base::DictValue metadata_dict;
  SetFormFillTypes(metadata_dict, {FormFillType::kAddress, FormFillType::kPhone,
                                   FormFillType::kCreditCard});
  CriticalActionEntry entry;
  entry.metadata = base::WriteJson(metadata_dict).value();
  EXPECT_EQ(entry.metadata,
            R"({"form_fill_types":["ADDRESS","PHONE","CREDIT_CARD"]})");
  EXPECT_EQ(entry.GetFormFillTypes(),
            FormFillTypes({FormFillType::kAddress, FormFillType::kPhone,
                           FormFillType::kCreditCard}));
}

TEST(CriticalActionTypesTest, FormFillTypesIgnoresUnknownAndLegacyMetadata) {
  CriticalActionEntry entry;
  // "PASSPORT" stands in for a string written by a newer version.
  entry.metadata = R"({"form_fill_types":["NAME","PASSPORT",42]})";
  EXPECT_EQ(entry.GetFormFillTypes(), FormFillTypes({FormFillType::kName}));

  // A legacy `kFormFill` row.
  entry.metadata = R"({"filled_types":["NAME_FULL"]})";
  EXPECT_TRUE(entry.GetFormFillTypes().empty());

  entry.metadata = R"({"form_fill_types":"ADDRESS"})";
  EXPECT_TRUE(entry.GetFormFillTypes().empty());

  entry.metadata = "not json";
  EXPECT_TRUE(entry.GetFormFillTypes().empty());

  entry.metadata = "";
  EXPECT_TRUE(entry.GetFormFillTypes().empty());
}

TEST(CriticalActionTypesTest, ContactInfoLabelListsAttributes) {
  // Attributes are listed in `FormFillType` order, not in stored order.
  CriticalActionEntry contact_info;
  contact_info.action_type = ActionType::kFormFillContactInfo;
  contact_info.metadata =
      R"({"form_fill_types":["PHONE","EMAIL","NAME","ADDRESS"]})";
  EXPECT_EQ(contact_info.GetLabel(),
            "Contact info filled: Address, Name, Email, Phone number");

  // Without attributes, the group label is used.
  contact_info.metadata = "";
  EXPECT_EQ(contact_info.GetLabel(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_FILLED));

  // Attributes of another group are skipped.
  contact_info.metadata = R"({"form_fill_types":["ADDRESS","CREDIT_CARD"]})";
  EXPECT_EQ(contact_info.GetLabel(), "Contact info filled: Address");
  contact_info.metadata = R"({"form_fill_types":["CREDIT_CARD"]})";
  EXPECT_EQ(contact_info.GetLabel(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_FILLED));

  // Payments keeps its group label.
  CriticalActionEntry payments;
  payments.action_type = ActionType::kFormFillPayments;
  payments.metadata = R"({"form_fill_types":["CREDIT_CARD"]})";
  EXPECT_EQ(payments.GetLabel(),
            l10n_util::GetStringUTF8(
                IDS_HISTORY_CRITICAL_ACTION_CARD_DETAILS_FILLED));

  // Other types ignore "form_fill_types".
  CriticalActionEntry legacy;
  legacy.action_type = ActionType::kFormFill;
  legacy.metadata = R"({"form_fill_types":["PHONE"]})";
  EXPECT_EQ(legacy.GetLabel(),
            l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_FILLED));
}

TEST(CriticalActionTypesTest, AddFormFillTypesMergesWithoutDuplicates) {
  CriticalActionEntry entry;
  entry.metadata = R"({"form_fill_types":["ADDRESS"],"other":1})";
  entry.AddFormFillTypes({FormFillType::kAddress, FormFillType::kName});
  EXPECT_EQ(entry.metadata,
            R"({"form_fill_types":["ADDRESS","NAME"],"other":1})");
}

TEST(CriticalActionTypesTest, AddFormFillTypesToEmptyMetadata) {
  CriticalActionEntry entry;
  entry.AddFormFillTypes({FormFillType::kEmail});
  EXPECT_EQ(entry.metadata, R"({"form_fill_types":["EMAIL"]})");

  // Adding nothing leaves the metadata unchanged.
  entry.AddFormFillTypes({});
  EXPECT_EQ(entry.metadata, R"({"form_fill_types":["EMAIL"]})");
}

TEST(CriticalActionTypesTest, AddFormFillTypesDropsUnknownStrings) {
  CriticalActionEntry entry;
  // "PASSPORT" stands in for a string written by a newer version.
  entry.metadata = R"({"form_fill_types":["NAME","PASSPORT",42]})";
  entry.AddFormFillTypes({FormFillType::kEmail});
  EXPECT_EQ(entry.metadata, R"({"form_fill_types":["NAME","EMAIL"]})");
}

TEST(CriticalActionTypesTest, AddFormFillTypesToMalformedMetadata) {
  CriticalActionEntry entry;
  // Metadata that isn't a JSON object is replaced.
  entry.metadata = "not json";
  entry.AddFormFillTypes({FormFillType::kPhone});
  EXPECT_EQ(entry.metadata, R"({"form_fill_types":["PHONE"]})");

  // A "form_fill_types" value that isn't a list is replaced; other keys are
  // kept.
  entry.metadata = R"({"form_fill_types":"ADDRESS","other":1})";
  entry.AddFormFillTypes({FormFillType::kPhone});
  EXPECT_EQ(entry.metadata, R"({"form_fill_types":["PHONE"],"other":1})");
}

}  // namespace
}  // namespace critical_actions
