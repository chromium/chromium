// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"

#include <string>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/webui/web_ui_util.h"

void RegisterNearbySharedStrings(content::WebUIDataSource* data_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"nearbyShareAccountRowLabel", IDS_NEARBY_ACCOUNT_ROW_LABEL_PH},
      {"nearbyShareActionsAccept", IDS_NEARBY_ACTIONS_ACCEPT},
      {"nearbyShareActionsCancel", IDS_NEARBY_ACTIONS_CANCEL},
      {"nearbyShareActionsClose", IDS_NEARBY_ACTIONS_CLOSE},
      {"nearbyShareActionsConfirm", IDS_NEARBY_ACTIONS_CONFIRM},
      {"nearbyShareActionsDecline", IDS_NEARBY_ACTIONS_DECLINE},
      {"nearbyShareActionsNext", IDS_NEARBY_ACTIONS_NEXT},
      {"nearbyShareActionsReject", IDS_NEARBY_ACTIONS_REJECT},
      {"nearbyShareConfirmationPageAddContactSubtitle",
       IDS_NEARBY_CONFIRMATION_PAGE_ADD_CONTACT_SUBTITLE},
      {"nearbyShareConfirmationPageAddContactTitle",
       IDS_NEARBY_CONFIRMATION_PAGE_ADD_CONTACT_TITLE},
      {"nearbyShareConfirmationPageTitle", IDS_NEARBY_CONFIRMATION_PAGE_TITLE},
      {"nearbyShareContactVisibilityAll", IDS_NEARBY_VISIBLITY_ALL_CONTACTS},
      {"nearbyShareContactVisibilityAllDescription",
       IDS_NEARBY_VISIBLITY_ALL_CONTACTS_DESCRIPTION},
      {"nearbyShareAllContactsToggle",
       IDS_NEARBY_VISIBILITY_ALL_CONTACTS_TOGGLE},
      {"nearbyShareContactVisiblityContactsButton",
       IDS_NEARBY_VISIBILITY_CONTACTS_BUTTON},
      {"nearbyShareContactVisibilityDownloadFailed",
       IDS_NEARBY_CONTACT_VISIBILITY_DOWNLOAD_FAILED},
      {"nearbyShareContactVisibilityDownloading",
       IDS_NEARBY_CONTACT_VISIBILITY_DOWNLOADING},
      {"nearbyShareContactVisibilityNoContactsTitle",
       IDS_NEARBY_CONTACT_VISIBILITY_NO_CONTACTS_TITLE},
      {"nearbyShareContactVisibilityNone", IDS_NEARBY_VISIBLITY_HIDDEN},
      {"nearbyShareContactVisibilityNoneDescription",
       IDS_NEARBY_VISIBLITY_HIDDEN_DESCRIPTION},
      {"nearbyShareContactVisibilityOwnAll",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_ALL},
      {"nearbyShareContactVisibilityOwnAllSelfShare",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_ALL_SELF_SHARE},
      {"nearbyShareContactVisibilityOwnNone",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_NONE},
      {"nearbyShareContactVisibilityOwnSome",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_SOME},
      {"nearbyShareContactVisibilityOwnSomeSelfShare",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_SOME_SELF_SHARE},
      {"nearbyShareContactVisibilityOwnYourDevices",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_YOUR_DEVICES},
      {"nearbyShareContactVisibilitySome", IDS_NEARBY_VISIBLITY_SOME_CONTACTS},
      {"nearbyShareContactVisibilitySomeDescription",
       IDS_NEARBY_VISIBLITY_SOME_CONTACTS_DESCRIPTION},
      {"nearbyShareContactVisibilityYourDevices",
       IDS_NEARBY_VISIBILITY_YOUR_DEVICES},
      {"nearbyShareContactVisibilityYourDevicesDescription",
       IDS_NEARBY_VISIBILITY_YOUR_DEVICES_DESCRIPTION},
      {"nearbyShareContactVisibilityUnknown", IDS_NEARBY_VISIBLITY_UNKNOWN},
      {"nearbyShareContactVisibilityUnknownDescription",
       IDS_NEARBY_VISIBLITY_UNKNOWN_DESCRIPTION},
      {"nearbyShareContactVisibilityZeroStateText",
       IDS_NEARBY_CONTACT_VISIBILITY_ZERO_STATE_TEXT},
      {"nearbyShareDeviceNameEmptyError", IDS_NEARBY_DEVICE_NAME_EMPTY_ERROR},
      {"nearbyShareDeviceNameTooLongError",
       IDS_NEARBY_DEVICE_NAME_TOO_LONG_ERROR},
      {"nearbyShareDeviceNameInvalidCharactersError",
       IDS_NEARBY_DEVICE_NAME_INVALID_CHARACTERS_ERROR},
      {"nearbyShareDiscoveryPageInfo", IDS_NEARBY_DISCOVERY_PAGE_INFO},
      {"nearbyShareDiscoveryPagePlaceholder",
       IDS_NEARBY_DISCOVERY_PAGE_PLACEHOLDER},
      {"nearbyShareDiscoveryPageSubtitle", IDS_NEARBY_DISCOVERY_PAGE_SUBTITLE},
      {"nearbyShareErrorCancelled", IDS_NEARBY_ERROR_CANCELLED},
      {"nearbyShareErrorCantReceive", IDS_NEARBY_ERROR_CANT_RECEIVE},
      {"nearbyShareErrorCantShare", IDS_NEARBY_ERROR_CANT_SHARE},
      {"nearbyShareErrorNoResponse", IDS_NEARBY_ERROR_NO_RESPONSE},
      {"nearbyShareErrorNotEnoughSpace", IDS_NEARBY_ERROR_NOT_ENOUGH_SPACE},
      {"nearbyShareErrorTransferInProgress",
       IDS_NEARBY_ERROR_TRANSFER_IN_PROGRESS},
      {"nearbyShareErrorRejected", IDS_NEARBY_ERROR_REJECTED},
      {"nearbyShareErrorSomethingWrong", IDS_NEARBY_ERROR_SOMETHING_WRONG},
      {"nearbyShareErrorTimeOut", IDS_NEARBY_ERROR_TIME_OUT},
      {"nearbyShareErrorTryAgain", IDS_NEARBY_ERROR_TRY_AGAIN},
      {"nearbyShareErrorUnsupportedFileType",
       IDS_NEARBY_ERROR_UNSUPPORTED_FILE_TYPE},
      {"nearbyShareOnboardingPageDeviceName",
       IDS_NEARBY_ONBOARDING_PAGE_DEVICE_NAME},
      {"nearbyShareOnboardingPageDeviceNameHelp",
       IDS_NEARBY_ONBOARDING_PAGE_DEVICE_NAME_HELP},
      {"nearbyShareOnboardingPageDeviceVisibility",
       IDS_NEARBY_ONBOARDING_PAGE_DEVICE_VISIBILITY},
      {"nearbyShareOnboardingPageDeviceVisibilityHelpAllContacts",
       IDS_NEARBY_ONBOARDING_PAGE_DEVICE_VISIBILITY_HELP_ALL_CONTACTS},
      {"nearbyShareOnboardingPageSubtitle",
       IDS_NEARBY_ONBOARDING_PAGE_SUBTITLE},
      {"nearbySharePreviewMultipleFileTitle",
       IDS_NEARBY_PREVIEW_TITLE_MULTIPLE_FILE},
      {"nearbyShareSecureConnectionId", IDS_NEARBY_SECURE_CONNECTION_ID},
      {"nearbyShareSettingsHelpCaptionBottom",
       IDS_NEARBY_SETTINGS_HELP_CAPTION_BOTTOM},
      {"nearbyShareVisibilityPageManageContacts",
       IDS_NEARBY_VISIBILITY_PAGE_MANAGE_CONTACTS},
      {"nearbyShareVisibilityPageSubtitle",
       IDS_NEARBY_VISIBILITY_PAGE_SUBTITLE},
      {"nearbyShareVisibilityPageTitle", IDS_NEARBY_VISIBILITY_PAGE_TITLE},
      {"nearbyShareHighVisibilitySubTitle",
       IDS_NEARBY_HIGH_VISIBILITY_SUB_TITLE},
      {"nearbyShareHighVisibilitySubTitleMinutes",
       IDS_NEARBY_HIGH_VISIBILITY_SUB_TITLE_MINUTES},
      {"nearbyShareHighVisibilitySubTitleSeconds",
       IDS_NEARBY_HIGH_VISIBILITY_SUB_TITLE_SECONDS},
      {"nearbyShareHighVisibilityHelpText",
       IDS_NEARBY_HIGH_VISIBILITY_HELP_TEXT},
      {"nearbyShareHighVisibilityTimeoutText",
       IDS_NEARBY_HIGH_VISIBILITY_TIMEOUT_TEXT},
      {"nearbyShareReceiveConfirmPageTitle",
       IDS_NEARBY_RECEIVE_CONFIRM_PAGE_TITLE},
      {"nearbyShareReceiveConfirmPageConnectionId",
       IDS_NEARBY_RECEIVE_CONFIRM_PAGE_CONNECTION_ID},
      {"nearbyShareErrorNoConnectionMedium",
       IDS_NEARBY_HIGH_VISIBILITY_CONNECTION_MEDIUM_ERROR},
      {"nearbyShareErrorTransferInProgressTitle",
       IDS_NEARBY_HIGH_VISIBILITY_TRANSFER_IN_PROGRESS_ERROR},
      {"nearbyShareErrorTransferInProgressDescription",
       IDS_NEARBY_HIGH_VISIBILITY_TRANSFER_IN_PROGRESS_DESCRIPTION},
      {"quickShareV2VisibilitySectionTitle",
       IDS_QUICK_SHARE_V2_VISIBILITY_SECTION_TITLE},
      {"quickShareV2VisibilitySectionSubtitleOnDisabled",
       IDS_QUICK_SHARE_V2_VISIBILITY_SECTION_SUBTITLE_ON_DISABLED},
      {"quickShareV2VisibilityYourDevicesSublabel",
       IDS_QUICK_SHARE_V2_VISIBILITY_YOUR_DEVICES_SUBLABEL},
      {"quickShareV2VisibilityContactsSublabel",
       IDS_QUICK_SHARE_V2_VISIBILITY_CONTACTS_SUBLABEL},
      {"quickShareV2VisibilityEveryoneLabel",
       IDS_QUICK_SHARE_V2_VISIBILITY_EVERYONE_LABEL},
      {"quickShareV2VisibilityEveryoneSublabel",
       IDS_QUICK_SHARE_V2_VISIBILITY_EVERYONE_SUBLABEL},
      {"quickShareV2VisibilityOnlyForTenMinutesLabel",
       IDS_QUICK_SHARE_V2_VISIBILITY_ONLY_FOR_TEN_MINUTES_LABEL}};
  data_source->AddLocalizedStrings(kLocalizedStrings);

  data_source->AddString("nearbyShareLearnMoreLink",
                         chrome::kNearbyShareLearnMoreURL);

  data_source->AddString("nearbyShareManageContactsUrl",
                         chrome::kNearbyShareManageContactsURL);

  if (features::IsNameEnabled()) {
    static constexpr webui::LocalizedString kLocalizedPlaceholderStringPairs[] =
        {
            {"nearbyShareContactVisibilityNoContactsSubtitle",
             IDS_NEARBY_CONTACT_VISIBILITY_NO_CONTACTS_SUBTITLE_PH},
            {"nearbyShareDiscoveryPageTitle",
             IDS_NEARBY_DISCOVERY_PAGE_TITLE_PH},
            {"nearbyShareOnboardingPageTitle",
             IDS_NEARBY_ONBOARDING_PAGE_TITLE_PH},
            {"nearbyShareFeatureName", IDS_NEARBY_SHARE_FEATURE_NAME_PH},
            {"nearbyShareErrorNoConnectionMediumDescription",
             IDS_NEARBY_HIGH_VISIBILITY_CONNECTION_MEDIUM_DESCRIPTION_PH},
            {"nearbyShareSettingsHelpCaptionTop",
             IDS_NEARBY_SETTINGS_HELP_CAPTION_TOP_PH},
        };

    for (const webui::LocalizedString string_pair :
         kLocalizedPlaceholderStringPairs) {
      data_source->AddString(
          string_pair.name,
          NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
              string_pair.id));
    }
  } else {
    static constexpr webui::LocalizedString kLocalizedStringPairs[] = {
        {"nearbyShareContactVisibilityNoContactsSubtitle",
         IDS_NEARBY_CONTACT_VISIBILITY_NO_CONTACTS_SUBTITLE},
        {"nearbyShareDiscoveryPageTitle", IDS_NEARBY_DISCOVERY_PAGE_TITLE},
        {"nearbyShareOnboardingPageTitle", IDS_NEARBY_ONBOARDING_PAGE_TITLE},
        {"nearbyShareFeatureName", IDS_NEARBY_SHARE_FEATURE_NAME},
        {"nearbyShareErrorNoConnectionMediumDescription",
         IDS_NEARBY_HIGH_VISIBILITY_CONNECTION_MEDIUM_DESCRIPTION},
        {"nearbyShareSettingsHelpCaptionTop",
         IDS_NEARBY_SETTINGS_HELP_CAPTION_TOP},
    };

    data_source->AddLocalizedStrings(kLocalizedStringPairs);
  }
}
