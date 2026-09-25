// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_types.h"

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

}  // namespace
}  // namespace critical_actions
