// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_pref_names.h"

namespace chromeos {
namespace prefs {

// Map of strings to values used for assistive input settings.
const char kAssistiveInputFeatureSettings[] =
    "assistive_input_feature_settings";

// A boolean pref of whether assist personal info is enabled.
const char kAssistPersonalInfoEnabled[] =
    "assistive_input.personal_info_enabled";

// A boolean pref of whether emoji suggestion is enabled.
const char kEmojiSuggestionEnabled[] =
    "assistive_input.emoji_suggestion_enabled";

// A boolean pref of whether emoji suggestion is enabled for managed device.
const char kEmojiSuggestionEnterpriseAllowed[] =
    "assistive_input.emoji_suggestion.enterprise_allowed";

// A dictionary pref to hold the mute setting for all the currently known
// audio devices.
const char kAudioDevicesMute[] = "settings.audio.devices.mute";

// A dictionary pref storing the gain settings for all the currently known
// audio input devices.
const char kAudioDevicesGainPercent[] = "settings.audio.devices.gain_percent";

// A dictionary pref storing the volume settings for all the currently known
// audio output devices.
const char kAudioDevicesVolumePercent[] =
    "settings.audio.devices.volume_percent";

// An integer pref to initially mute volume if 1. This pref is ignored if
// |kAudioOutputAllowed| is set to false, but its value is preserved, therefore
// when the policy is lifted the original mute state is restored.  This setting
// is here only for migration purposes now. It is being replaced by the
// |kAudioDevicesMute| setting.
const char kAudioMute[] = "settings.audio.mute";

// A pref holding the value of the policy used to disable playing audio on
// ChromeOS devices. This pref overrides |kAudioMute| but does not overwrite
// it, therefore when the policy is lifted the original mute state is restored.
const char kAudioOutputAllowed[] = "hardware.audio_output_enabled";

// A double pref storing the user-requested volume. This setting is here only
// for migration purposes now. It is being replaced by the
// |kAudioDevicesVolumePercent| setting.
const char kAudioVolumePercent[] = "settings.audio.volume_percent";

// A dictionary pref that maps stable device id string to |AudioDeviceState|.
// Different state values indicate whether or not a device has been selected
// as the active one for audio I/O, or it's a new plugged device.
const char kAudioDevicesState[] = "settings.audio.device_state";

// A string pref storing an identifier that is getting sent with parental
// consent in EDU account addition flow.
const char kEduCoexistenceId[] = "account_manager.edu_coexistence_id";

// A string pref storing a parental consent text version that requires
// invalidation of the secondary accounts added with the previous consent
// versions.
const char kEduCoexistenceSecondaryAccountsInvalidationVersion[] =
    "account_manager.edu_coexistence_secondary_accounts_invalidation_version";

// A string pref containing valid version of Edu Coexistence Terms of Service.
// Controlled by EduCoexistenceToSVersion policy.
const char kEduCoexistenceToSVersion[] =
    "family_link_user.edu_coexistence_tos_version";

// A dictionary of info for Quirks Client/Server interaction, mostly last server
// request times, keyed to display product_id's.
const char kQuirksClientLastServerCheck[] = "quirks_client.last_server_check";

// Whether 802.11r Fast BSS Transition is currently enabled.
const char kDeviceWiFiFastTransitionEnabled[] =
    "net.device_wifi_fast_transition_enabled";

// A boolean pref to store if Secondary Google Account additions are allowed on
// Chrome OS Account Manager. The default value is |true|, i.e. Secondary Google
// Account additions are allowed by default.
const char kSecondaryGoogleAccountSigninAllowed[] =
    "account_manager.secondary_google_account_signin_allowed";

// The following SAML-related prefs are not settings that the domain admin can
// set, but information that the SAML Identity Provider can send us:

// A time pref - when the SAML password was last set, according to the SAML IdP.
const char kSamlPasswordModifiedTime[] = "saml.password_modified_time";
// A time pref - when the SAML password did expire, or will expire, according to
// the SAML IDP.
const char kSamlPasswordExpirationTime[] = "saml.password_expiration_time";
// A string pref - the URL where the user can update their password, according
// to the SAML IdP.
const char kSamlPasswordChangeUrl[] = "saml.password_change_url";

// Boolean pref indicating whether the user has completed (or skipped) the
// out-of-box experience (OOBE) sync consent screen. Before this pref is set
// both OS and browser sync will be disabled. After this pref is set it's
// possible to check sync state to see if the user enabled it.
const char kSyncOobeCompleted[] = "sync.oobe_completed";

// Boolean pref indicating whether a user has enabled the display password
// button on the login/lock screen.
const char kLoginDisplayPasswordButtonEnabled[] =
    "login_display_password_button_enabled";

// Boolean pref indicating whether the user has enabled Suggested Content.
const char kSuggestedContentEnabled[] = "settings.suggested_content_enabled";

}  // namespace prefs
}  // namespace chromeos
