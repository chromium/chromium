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
#include "components/security_interstitials/core/utils.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace supervised_user {

namespace {

static const int kAvatarSize1x = 36;
static const int kAvatarSize2x = 72;

std::string BuildAvatarImageUrl(const std::string& url, int size) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return url;

  GURL to_return =
      signin::GetAvatarImageURLWithOptions(gurl, size, /*no_silhouette=*/false);
  return to_return.spec();
}

}  //  namespace

int GetBlockMessageID(FilteringBehaviorReason reason, bool single_parent) {
  switch (reason) {
    case FilteringBehaviorReason::DEFAULT:
      return single_parent ? IDS_CHILD_BLOCK_MESSAGE_DEFAULT_SINGLE_PARENT
                           : IDS_CHILD_BLOCK_MESSAGE_DEFAULT_MULTI_PARENT;
    case FilteringBehaviorReason::ASYNC_CHECKER:
      return IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES;
    case FilteringBehaviorReason::MANUAL:
      return single_parent ? IDS_CHILD_BLOCK_MESSAGE_MANUAL_SINGLE_PARENT
                           : IDS_CHILD_BLOCK_MESSAGE_MANUAL_MULTI_PARENT;
    case FilteringBehaviorReason::FILTER_DISABLED:
      NOTREACHED() << "When filtering is disabled, nothing is blocked";
  }
}

int GetInterstitialMessageID(FilteringBehaviorReason reason) {
  if (supervised_user::IsBlockInterstitialV3Enabled()) {
    // For the V3 interstitial, the filtering reason is included in the
    // interstitial message.
    switch (reason) {
      case FilteringBehaviorReason::DEFAULT:
        return IDS_SUPERVISED_USER_INTERSTITIAL_MESSAGE_BLOCK_ALL;
      case FilteringBehaviorReason::ASYNC_CHECKER:
        return IDS_SUPERVISED_USER_INTERSTITIAL_MESSAGE_SAFE_SITES;
      case FilteringBehaviorReason::MANUAL:
        return IDS_SUPERVISED_USER_INTERSTITIAL_MESSAGE_MANUAL;
      case FilteringBehaviorReason::FILTER_DISABLED:
        NOTREACHED() << "When filtering is disabled, nothing is blocked";
    }
  }
  return IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_V2;
}

#if BUILDFLAG(IS_ANDROID)
std::string BuildErrorPageHtmlWithoutApprovals(const GURL& url,
                                               const std::string& app_locale) {
  base::Value::Dict load_time_data;
  load_time_data.Set("blockPageTitle",
                     l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_TITLE));
  load_time_data.Set("blockPageHeader",
                     l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_TITLE));
  load_time_data.Set("blockPageMessage",
                     l10n_util::FormatString(
                         l10n_util::GetStringUTF16(IDS_NO_APPROVALS_MESSAGE),
                         {base::UTF8ToUTF16(url.GetHost())}, nullptr));
  load_time_data.Set("learnMore", l10n_util::GetStringUTF8(
                                      IDS_NO_APPROVALS_LEARN_MORE_BUTTON));
  load_time_data.Set("backButton",
                     l10n_util::GetStringUTF8(IDS_NO_APPROVALS_BACK_BUTTON));

  webui::SetLoadTimeDataDefaults(app_locale, &load_time_data);

  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_NO_APPROVALS_HTML);
  webui::AppendWebUiCssTextDefaults(&html);
  std::string error_html = webui::GetI18nTemplateHtml(html, load_time_data);
  return error_html;
}
#endif  // BUILDFLAG(IS_ANDROID)

std::string BuildErrorPageHtmlWithApprovals(
    bool allow_access_requests,
    std::optional<Custodian> custodian,
    std::optional<Custodian> second_custodian,
    FilteringBehaviorReason reason,
    const std::string& app_locale,
    bool already_sent_remote_request,
    bool is_main_frame,
    std::optional<float> ios_font_size_multiplier) {
  base::Value::Dict load_time_data;
  load_time_data.Set("blockPageTitle",
                     l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_TITLE));
  load_time_data.Set("allowAccessRequests", allow_access_requests);

  if (custodian.has_value()) {
    load_time_data.Set("custodianName", custodian->GetName());
    load_time_data.Set("custodianEmail", custodian->GetEmailAddress());
    load_time_data.Set(
        "avatarURL1x",
        BuildAvatarImageUrl(custodian->GetProfileImageUrl(), kAvatarSize1x));
    load_time_data.Set(
        "avatarURL2x",
        BuildAvatarImageUrl(custodian->GetProfileImageUrl(), kAvatarSize2x));
  } else {
    // empty custodianName denotes no custodian, see
    // components/supervised_user/core/browser/resources/supervised_user_block_interstitial_v2.js
    load_time_data.Set("custodianName", "");
  }

  if (second_custodian.has_value()) {
    load_time_data.Set("secondCustodianName", second_custodian->GetName());
    load_time_data.Set("secondCustodianEmail",
                       second_custodian->GetEmailAddress());
    load_time_data.Set(
        "secondAvatarURL1x",
        BuildAvatarImageUrl(second_custodian->GetProfileImageUrl(),
                            kAvatarSize1x));
    load_time_data.Set(
        "secondAvatarURL2x",
        BuildAvatarImageUrl(second_custodian->GetProfileImageUrl(),
                            kAvatarSize2x));
  } else {
    // empty secondCustodianName denotes no second custodian, see
    // components/supervised_user/core/browser/resources/supervised_user_block_interstitial_v2.js
    load_time_data.Set("secondCustodianName", "");
  }

  load_time_data.Set("alreadySentRemoteRequest", already_sent_remote_request);
  load_time_data.Set("isMainFrame", is_main_frame);

  bool local_web_approvals_enabled =
      (is_main_frame && supervised_user::IsLocalWebApprovalsEnabled()) ||
      (!is_main_frame &&
       supervised_user::IsLocalWebApprovalsEnabledForSubframes());
  load_time_data.Set("isLocalWebApprovalsEnabled", local_web_approvals_enabled);

  std::string block_page_header;
  std::string block_page_message;
  if (allow_access_requests) {
    block_page_header =
        l10n_util::GetStringUTF8(IDS_CHILD_BLOCK_INTERSTITIAL_HEADER);
    block_page_message =
        l10n_util::GetStringUTF8(GetInterstitialMessageID(reason));
  } else {
    block_page_header = l10n_util::GetStringUTF8(
        IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED);
  }

  load_time_data.Set("blockPageHeader", block_page_header);
  load_time_data.Set("blockPageMessage", block_page_message);

  if (!supervised_user::IsBlockInterstitialV3Enabled()) {
    // For the V2 interstitial, the filtering reason is displayed in a
    // separate UI component.
    load_time_data.Set(
        "blockReasonMessage",
        l10n_util::GetStringUTF8(GetBlockMessageID(
            reason, /*single_parent=*/!second_custodian.has_value())));
    load_time_data.Set(
        "blockReasonHeader",
        l10n_util::GetStringUTF8(IDS_SUPERVISED_USER_BLOCK_HEADER));
    load_time_data.Set("siteBlockHeader",
                       l10n_util::GetStringUTF8(IDS_GENERIC_SITE_BLOCK_HEADER));
    load_time_data.Set(
        "showDetailsLink",
        l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_SHOW_DETAILS));
    load_time_data.Set(
        "hideDetailsLink",
        l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_HIDE_DETAILS));
  }

  load_time_data.Set(
      "remoteApprovalsButton",
      l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_ASK_IN_A_MESSAGE_BUTTON));
  load_time_data.Set("backButton",
                     l10n_util::GetStringUTF8(IDS_REQUEST_SENT_OK));

  load_time_data.Set(
      "localApprovalsButton",
      l10n_util::GetStringUTF8(IDS_BLOCK_INTERSTITIAL_ASK_IN_PERSON_BUTTON));
  load_time_data.Set("localApprovalsRemoteRequestSentButton",
                     l10n_util::GetStringUTF8(
                         IDS_BLOCK_INTERSTITIAL_ASK_IN_PERSON_INSTEAD_BUTTON));

  std::string request_sent_message;
  std::string request_failed_message;
  std::string request_sent_description;
  request_sent_message = l10n_util::GetStringUTF8(
      IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_MESSAGE);
  request_sent_description = l10n_util::GetStringUTF8(
      !second_custodian.has_value()
          ? IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_SINGLE_PARENT
          : IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_MULTI_PARENT);
  request_failed_message = l10n_util::GetStringUTF8(
      !second_custodian.has_value()
          ? IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT
          : IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT);

  load_time_data.Set("requestSentMessage", std::move(request_sent_message));
  load_time_data.Set("requestSentDescription",
                     std::move(request_sent_description));
  load_time_data.Set("requestFailedMessage", std::move(request_failed_message));
  webui::SetLoadTimeDataDefaults(app_locale, &load_time_data);

  if (ios_font_size_multiplier) {
    security_interstitials::AdjustFontSize(load_time_data,
                                           ios_font_size_multiplier.value());
  }

  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          supervised_user::IsBlockInterstitialV3Enabled()
              ? IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_V3_HTML
              : IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_V2_HTML);
  webui::AppendWebUiCssTextDefaults(&html);
  std::string error_html = webui::GetI18nTemplateHtml(html, load_time_data);
  return error_html;
}

}  //  namespace supervised_user
