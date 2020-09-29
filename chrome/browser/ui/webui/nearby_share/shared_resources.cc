// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"

#include <string>

#include "base/containers/span.h"
#include "base/logging.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/nearby_share_dialog_resources.h"
#include "chrome/grit/nearby_share_dialog_resources_map.h"
#include "chrome/grit/nearby_shared_resources.h"
#include "chrome/grit/nearby_shared_resources_map.h"
#include "chrome/grit/nearby_shared_resources_v3.h"
#include "chrome/grit/nearby_shared_resources_v3_map.h"
#include "ui/base/webui/web_ui_util.h"

const char kNearbyShareGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/nearby_share/";

namespace {

void RegisterResourcesWithPrefix(
    content::WebUIDataSource* data_source,
    const base::span<const GritResourceMap>& resources,
    std::string prefix) {
  std::string generate_path{kNearbyShareGeneratedPath};
  for (const GritResourceMap& resource : resources) {
    std::string path = resource.name;
    if (path.rfind(generate_path, 0) == 0) {
      path = path.substr(generate_path.size());
    } else {
      path = prefix + path;
    }
    data_source->AddResourcePath(path, resource.value);
  }
}

}  // namespace

void RegisterNearbySharedMojoResources(content::WebUIDataSource* data_source) {
  data_source->AddResourcePath("mojo/nearby_share.mojom-lite.js",
                               IDR_NEARBY_SHARE_MOJO_JS);
  data_source->AddResourcePath("mojo/nearby_share_target_types.mojom-lite.js",
                               IDR_NEARBY_SHARE_TARGET_TYPES_MOJO_JS);
  data_source->AddResourcePath("mojo/nearby_share_settings.mojom-lite.js",
                               IDR_NEARBY_SHARE_SETTINGS_MOJOM_LITE_JS);
}

void RegisterNearbySharedResources(content::WebUIDataSource* data_source) {
  RegisterResourcesWithPrefix(
      data_source,
      /*resources=*/
      base::make_span(kNearbySharedResources, kNearbySharedResourcesSize),
      /*prefix=*/"shared/");
  RegisterResourcesWithPrefix(
      data_source,
      /*resources=*/
      base::make_span(kNearbySharedResourcesV3, kNearbySharedResourcesV3Size),
      /*prefix=*/"shared/");
  RegisterNearbySharedMojoResources(data_source);
}

void RegisterNearbySharedStrings(content::WebUIDataSource* data_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"nearbyShareAccountRowLabel", IDS_NEARBY_ACCOUNT_ROW_LABEL},
      {"nearbyShareActionsCancel", IDS_NEARBY_ACTIONS_CANCEL},
      {"nearbyShareActionsConfirm", IDS_NEARBY_ACTIONS_CONFIRM},
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
      {"nearbyShareContactVisibilityContactsTitle",
       IDS_NEARBY_CONTACT_VISIBILITY_CONTACTS_TITLE},
      {"nearbyShareContactVisibilityNearbyShareOpenOthers",
       IDS_NEARBY_CONTACT_VISIBILITY_NEARBY_SHARE_OPEN_OTHERS},
      {"nearbyShareContactVisibilityNearbyShareOpenOwn",
       IDS_NEARBY_CONTACT_VISIBILITY_NEARBY_SHARE_OPEN_OWN},
      {"nearbyShareContactVisibilityNoContactsSubtitle",
       IDS_NEARBY_CONTACT_VISIBILITY_NO_CONTACTS_SUBTITLE},
      {"nearbyShareContactVisibilityNoContactsTitle",
       IDS_NEARBY_CONTACT_VISIBILITY_NO_CONTACTS_TITLE},
      {"nearbyShareContactVisibilityNone", IDS_NEARBY_VISIBLITY_HIDDEN},
      {"nearbyShareContactVisibilityNoneDescription",
       IDS_NEARBY_VISIBLITY_HIDDEN_DESCRIPTION},
      {"nearbyShareContactVisibilityOthers",
       IDS_NEARBY_CONTACT_VISIBILITY_OTHERS},
      {"nearbyShareContactVisibilityOthersTitle",
       IDS_NEARBY_CONTACT_VISIBILITY_OTHERS_TITLE},
      {"nearbyShareContactVisibilityOwnAll",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_ALL},
      {"nearbyShareContactVisibilityOwnNone",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_NONE},
      {"nearbyShareContactVisibilityOwnSome",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_SOME},
      {"nearbyShareContactVisibilityOwnTitle",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_TITLE},
      {"nearbyShareContactVisibilitySome", IDS_NEARBY_VISIBLITY_SOME_CONTACTS},
      {"nearbyShareContactVisibilitySomeDescription",
       IDS_NEARBY_VISIBLITY_SOME_CONTACTS_DESCRIPTION},
      {"nearbyShareContactVisibilityUnknown", IDS_NEARBY_VISIBLITY_UNKNOWN},
      {"nearbyShareContactVisibilityUnknownDescription",
       IDS_NEARBY_VISIBLITY_UNKNOWN_DESCRIPTION},
      {"nearbyShareContactVisibilityZeroStateInfo",
       IDS_NEARBY_CONTACT_VISIBILITY_ZERO_STATE_INFO},
      {"nearbyShareContactVisibilityZeroStateText",
       IDS_NEARBY_CONTACT_VISIBILITY_ZERO_STATE_TEXT},
      {"nearbyShareDeviceNameEmptyError", IDS_NEARBY_DEVICE_NAME_EMPTY_ERROR},
      {"nearbyShareDeviceNameTooLongError",
       IDS_NEARBY_DEVICE_NAME_TOO_LONG_ERROR},
      {"nearbyShareDeviceNameInvalidCharactersError",
       IDS_NEARBY_DEVICE_NAME_INVALID_CHARACTERS_ERROR},
      {"nearbyShareDiscoveryPageInfo", IDS_NEARBY_DISCOVERY_PAGE_INFO},
      {"nearbyShareDiscoveryPageSubtitle", IDS_NEARBY_DISCOVERY_PAGE_SUBTITLE},
      {"nearbyShareDiscoveryPageTitle", IDS_NEARBY_DISCOVERY_PAGE_TITLE},
      {"nearbyShareFeatureName", IDS_NEARBY_SHARE_FEATURE_NAME},
      {"nearbyShareOnboardingPageDeviceName",
       IDS_NEARBY_ONBOARDING_PAGE_DEVICE_NAME},
      {"nearbyShareOnboardingPageSubtitle",
       IDS_NEARBY_ONBOARDING_PAGE_SUBTITLE},
      {"nearbyShareOnboardingPageTitle", IDS_NEARBY_ONBOARDING_PAGE_TITLE},
      {"nearbyShareSecureConnectionId", IDS_NEARBY_SECURE_CONNECTION_ID},
      {"nearbyShareVisibilityPageManageContacts",
       IDS_NEARBY_VISIBILITY_PAGE_MANAGE_CONTACTS},
      {"nearbyShareVisibilityPageSubtitle",
       IDS_NEARBY_VISIBILITY_PAGE_SUBTITLE},
      {"nearbyShareVisibilityPageTitle", IDS_NEARBY_VISIBILITY_PAGE_TITLE},
      {"nearbyShareHighVisibilitySubTitle",
       IDS_NEARBY_HIGH_VISIBILITY_SUB_TITLE},
      {"nearbyShareHighVisibilityHelpText",
       IDS_NEARBY_HIGH_VISIBILITY_HELP_TEXT},
      {"nearbyShareReceiveConfirmPageTitle",
       IDS_NEARBY_RECEIVE_CONFIRM_PAGE_TITLE}};
  webui::AddLocalizedStringsBulk(data_source, kLocalizedStrings);
}
