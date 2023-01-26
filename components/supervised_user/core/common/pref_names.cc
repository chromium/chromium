// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/pref_names.h"

namespace prefs {

// Stores the email address associated with the google account of the custodian
// of the supervised user, set when the supervised user is created.
const char kSupervisedUserCustodianEmail[] = "profile.managed.custodian_email";

// Stores the display name associated with the google account of the custodian
// of the supervised user, updated (if possible) each time the supervised user
// starts a session.
const char kSupervisedUserCustodianName[] = "profile.managed.custodian_name";

// Stores the obfuscated gaia id associated with the google account of the
// custodian of the supervised user, updated (if possible) each time the
// supervised user starts a session.
const char kSupervisedUserCustodianObfuscatedGaiaId[] =
    "profile.managed.custodian_obfuscated_gaia_id";

// Stores the URL of the profile image associated with the google account of the
// custodian of the supervised user.
const char kSupervisedUserCustodianProfileImageURL[] =
    "profile.managed.custodian_profile_image_url";

// Stores the URL of the profile associated with the google account of the
// custodian of the supervised user.
const char kSupervisedUserCustodianProfileURL[] =
    "profile.managed.custodian_profile_url";

// Stores the email address associated with the google account of the secondary
// custodian of the supervised user, set when the supervised user is created.
const char kSupervisedUserSecondCustodianEmail[] =
    "profile.managed.second_custodian_email";

// Stores the display name associated with the google account of the secondary
// custodian of the supervised user, updated (if possible) each time the
// supervised user starts a session.
const char kSupervisedUserSecondCustodianName[] =
    "profile.managed.second_custodian_name";

// Stores the obfuscated gaia id associated with the google account of the
// secondary custodian of the supervised user, updated (if possible) each time
// the supervised user starts a session.
const char kSupervisedUserSecondCustodianObfuscatedGaiaId[] =
    "profile.managed.second_custodian_obfuscated_gaia_id";

// Stores the URL of the profile image associated with the google account of the
// secondary custodian of the supervised user.
const char kSupervisedUserSecondCustodianProfileImageURL[] =
    "profile.managed.second_custodian_profile_image_url";

// Stores the URL of the profile associated with the google account of the
// secondary custodian of the supervised user.
const char kSupervisedUserSecondCustodianProfileURL[] =
    "profile.managed.second_custodian_profile_url";

// Whether the supervised user may approve extension permission requests. If
// false, extensions should not be able to request new permissions, and new
// extensions should not be installable.
const char kSupervisedUserExtensionsMayRequestPermissions[] =
    "profile.managed.extensions_may_request_permissions";

// Maps host names to whether the host is manually allowed or blocked.
const char kSupervisedUserManualHosts[] = "profile.managed.manual_hosts";

// Maps URLs to whether the URL is manually allowed or blocked.
const char kSupervisedUserManualURLs[] = "profile.managed.manual_urls";

// Stores whether the SafeSites filter is enabled.
const char kSupervisedUserSafeSites[] = "profile.managed.safe_sites";

// Stores settings that can be modified both by a supervised user and their
// manager. See SupervisedUserSharedSettingsService for a description of
// the format.
const char kSupervisedUserSharedSettings[] = "profile.managed.shared_settings";

}  // namespace prefs