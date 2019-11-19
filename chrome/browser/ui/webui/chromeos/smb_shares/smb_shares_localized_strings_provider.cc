// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_shares_localized_strings_provider.h"

#include "base/stl_util.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace smb_dialog {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr LocalizedString kLocalizedStrings[] = {
      // TODO(baileyberro): Rename these resources since they are no longer in
      // settings.
      {"smbShareUrl", IDS_SETTINGS_DOWNLOADS_ADD_SHARE_URL},
      {"smbShareName", IDS_SETTINGS_DOWNLOADS_ADD_SHARE_NAME},
      {"smbShareUsername", IDS_SETTINGS_DOWNLOADS_ADD_SHARE_USERNAME},
      {"smbSharePassword", IDS_SETTINGS_DOWNLOADS_ADD_SHARE_PASSWORD},
      {"smbShareSaveCredentials",
       IDS_SETTINGS_DOWNLOADS_ADD_SHARE_SAVE_CREDENTIALS},
      {"smbShareAuthenticationMethod",
       IDS_SETTINGS_DOWNLOADS_ADD_SHARE_AUTHENTICATION_METHOD},
      {"smbShareStandardAuthentication",
       IDS_SETTINGS_DOWNLOADS_ADD_SHARE_STANDARD_AUTHENTICATION},
      {"smbShareKerberosAuthentication",
       IDS_SETTINGS_DOWNLOADS_ADD_SHARE_KERBEROS_AUTHENTICATION},
      {"smbShareAddedSuccessfulMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_SUCCESS_MESSAGE},
      {"smbShareAddedErrorMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_ERROR_MESSAGE},
      {"smbShareAddedAuthFailedMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_AUTH_FAILED_MESSAGE},
      {"smbShareAddedNotFoundMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_NOT_FOUND_MESSAGE},
      {"smbShareAddedUnsupportedDeviceMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_UNSUPPORTED_DEVICE_MESSAGE},
      {"smbShareAddedMountExistsMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_EXISTS_MESSAGE},
      {"smbShareAddedInvalidURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_URL_MESSAGE},
      {"smbShareAddedInvalidSSOURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_SSO_URL_MESSAGE},
      {"smbShareDiscoveryMessage",
       IDS_SETTINGS_DOWNLOADS_ADD_SHARE_DISCOVERY_MESSAGE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings,
                          base::size(kLocalizedStrings));
}

}  // namespace smb_dialog
}  // namespace chromeos
