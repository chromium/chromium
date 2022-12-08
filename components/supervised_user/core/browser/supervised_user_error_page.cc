// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_error_page.h"

#include <string>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace supervised_user_error_page {

namespace {

static const int kAvatarSize1x = 36;
static const int kAvatarSize2x = 72;

bool ReasonIsAutomatic(FilteringBehaviorReason reason) {
  return reason == ASYNC_CHECKER || reason == DENYLIST;
}

std::string BuildAvatarImageUrl(const std::string& url, int size) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return url;

  GURL to_return = signin::GetAvatarImageURLWithOptions(
      gurl, size, false /* no_silhouette */);
  return to_return.spec();
}

}  //  namespace

int GetBlockMessageID(FilteringBehaviorReason reason, bool single_parent) {
  switch (reason) {
    case DEFAULT:
      return single_parent ? IDS_CHILD_BLOCK_MESSAGE_DEFAULT_SINGLE_PARENT
                           : IDS_CHILD_BLOCK_MESSAGE_DEFAULT_MULTI_PARENT;
    case DENYLIST:
    case ASYNC_CHECKER:
      return IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES;
    case ALLOWLIST:
      NOTREACHED();
      break;
    case MANUAL:
      return single_parent ? IDS_CHILD_BLOCK_MESSAGE_MANUAL_SINGLE_PARENT
                           : IDS_CHILD_BLOCK_MESSAGE_MANUAL_MULTI_PARENT;
    case NOT_SIGNED_IN:
      return IDS_SUPERVISED_USER_NOT_SIGNED_IN;
  }
  NOTREACHED();
  return 0;
}

std::string BuildHtml(bool allow_access_requests,
                      const std::string& profile_image_url,
                      const std::string& profile_image_url2,
                      const std::string& custodian,
                      const std::string& custodian_email,
                      const std::string& second_custodian,
                      const std::string& second_custodian_email,
                      FilteringBehaviorReason reason,
                      const std::string& app_locale,
                      bool already_sent_remote_request,
                      bool is_main_frame) {
  base::Value::Dict strings;
  strings.Set("blockPageTitle",
              l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_TITLE));
  strings.Set("allowAccessRequests", allow_access_requests);
  strings.Set("avatarURL1x",
              BuildAvatarImageUrl(profile_image_url, kAvatarSize1x));
  strings.Set("avatarURL2x",
              BuildAvatarImageUrl(profile_image_url, kAvatarSize2x));
  strings.Set("secondAvatarURL1x",
              BuildAvatarImageUrl(profile_image_url2, kAvatarSize1x));
  strings.Set("secondAvatarURL2x",
              BuildAvatarImageUrl(profile_image_url2, kAvatarSize2x));
  strings.Set("custodianName", custodian);
  strings.Set("custodianEmail", custodian_email);
  strings.Set("secondCustodianName", second_custodian);
  strings.Set("secondCustodianEmail", second_custodian_email);
  strings.Set("alreadySentRemoteRequest", already_sent_remote_request);
  strings.Set("isMainFrame", is_main_frame);
  bool web_filter_interstitial_refresh_enabled =
      supervised_users::IsWebFilterInterstitialRefreshEnabled();
  bool local_web_approvals_enabled =
      supervised_users::IsLocalWebApprovalsEnabled();
  strings.Set("isWebFilterInterstitialRefreshEnabled",
              web_filter_interstitial_refresh_enabled);
  strings.Set("isLocalWebApprovalsEnabled", local_web_approvals_enabled);
  strings.Set("isLocalWebApprovalsPreferred",
              supervised_users::IsLocalWebApprovalThePreferredButton());
  bool is_automatically_blocked = ReasonIsAutomatic(reason);

  std::string block_header;
  std::string block_message;
  if (reason == FilteringBehaviorReason::NOT_SIGNED_IN) {
    block_header =
        l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_HEADER_NOT_SIGNED_IN);
  } else if (allow_access_requests) {
    block_header =
        l10n_util::GetStringUTF8(IDS_CHILD_BLOCK_INTERSTITIAL_HEADER);
    block_message =
        l10n_util::GetStringUTF8(web_filter_interstitial_refresh_enabled
                                     ? IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_V2
                                     : IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE);
  } else {
    block_header = l10n_util::GetStringUTF8(
        IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED);
  }
  strings.Set("blockPageHeader", block_header);
  strings.Set("blockPageMessage", block_message);
  strings.Set("blockReasonMessage", l10n_util::GetStringUTF8(GetBlockMessageID(
                                        reason, second_custodian.empty())));
  strings.Set("blockReasonHeader",
              l10n_util::GetStringUTF8(IDS_SUPERVISED_USER_BLOCK_HEADER));
  strings.Set("siteBlockHeader",
              l10n_util::GetStringUTF8(IDS_GENERIC_SITE_BLOCK_HEADER));

  strings.Set("showFeedbackLink", is_automatically_blocked);
  strings.Set("feedbackLink",
              l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_SEND_FEEDBACK));
  if (web_filter_interstitial_refresh_enabled) {
    strings.Set(
        "remoteApprovalsButton",
        l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_SEND_MESSAGE_BUTTON));
    strings.Set("backButton", l10n_util::GetStringUTF8(IDS_REQUEST_SENT_OK));
  } else {
    strings.Set(
        "remoteApprovalsButton",
        l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_REQUEST_ACCESS_BUTTON));
    strings.Set("backButton", l10n_util::GetStringUTF8(IDS_BACK_BUTTON));
  }

  strings.Set(
      "localApprovalsButton",
      l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_ASK_IN_PERSON_BUTTON));
  strings.Set("localApprovalsRemoteRequestSentButton",
              l10n_util::GetStringUTF8(
                  IDS_BLOCK_INTERSTITIAL_ASK_IN_PERSON_INSTEAD_BUTTON));
  strings.Set("showDetailsLink",
              l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_SHOW_DETAILS));
  strings.Set("hideDetailsLink",
              l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_HIDE_DETAILS));
  std::string request_sent_message;
  std::string request_failed_message;
  std::string request_sent_description;
  if (web_filter_interstitial_refresh_enabled) {
    request_sent_message = l10n_util::GetStringUTF8(
        IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_MESSAGE);
    request_sent_description = l10n_util::GetStringUTF8(
        second_custodian.empty()
            ? IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_SINGLE_PARENT
            : IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_MULTI_PARENT);
    request_failed_message = l10n_util::GetStringUTF8(
        second_custodian.empty()
            ? IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT
            : IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT);
  } else if (second_custodian.empty()) {
    request_sent_message = l10n_util::GetStringUTF8(
        IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT);
    request_failed_message = l10n_util::GetStringUTF8(
        IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT);
  } else {
    request_sent_message = l10n_util::GetStringUTF8(
        IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT);
    request_failed_message = l10n_util::GetStringUTF8(
        IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT);
  }
  strings.Set("requestSentMessage", std::move(request_sent_message));
  strings.Set("requestSentDescription", std::move(request_sent_description));
  strings.Set("requestFailedMessage", std::move(request_failed_message));
  webui::SetLoadTimeDataDefaults(app_locale, &strings);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          web_filter_interstitial_refresh_enabled
              ? IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_V2_HTML
              : IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_HTML);
  webui::AppendWebUiCssTextDefaults(&html);
  std::string error_html = webui::GetI18nTemplateHtml(html, strings);
  return error_html;
}

}  //  namespace supervised_user_error_page
