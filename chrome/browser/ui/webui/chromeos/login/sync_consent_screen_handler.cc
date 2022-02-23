// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

// This helper function gets strings from WebUI and a set of known string
// resource ids, and converts strings back to IDs. It CHECKs if string is not
// found in resources.
void GetConsentIDs(const std::unordered_map<std::string, int>& known_strings,
                   const login::StringList& consent_description,
                   const std::string& consent_confirmation,
                   std::vector<int>* consent_description_ids,
                   int* consent_confirmation_id) {
  // The strings returned by the WebUI are not free-form, they must belong into
  // a pre-determined set of strings (stored in `string_to_grd_id_map_`). As
  // this has privacy and legal implications, CHECK the integrity of the strings
  // received from the renderer process before recording the consent.
  for (const std::string& text : consent_description) {
    auto iter = known_strings.find(text);
    CHECK(iter != known_strings.end()) << "Unexpected string:\n" << text;
    consent_description_ids->push_back(iter->second);
  }

  auto iter = known_strings.find(consent_confirmation);
  CHECK(iter != known_strings.end()) << "Unexpected string:\n"
                                     << consent_confirmation;
  *consent_confirmation_id = iter->second;
}

}  // namespace

namespace chromeos {

constexpr StaticOobeScreenId SyncConsentScreenView::kScreenId;

// TODO(https://crbug.com/1229582): Remove SplitSettings from names in this file
SyncConsentScreenHandler::SyncConsentScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.SyncConsentScreen.userActed");
}

SyncConsentScreenHandler::~SyncConsentScreenHandler() = default;

std::string Sanitize(const std::u16string& raw_string) {
  // When the strings are passed to the HTML, the Unicode NBSP symbol
  // (\u00A0) will be automatically replaced with "&nbsp;". This change must
  // be mirrored in the string-to-ids map. Note that "\u00A0" is actually two
  // characters, so we must use base::ReplaceSubstrings* rather than
  // base::ReplaceChars.
  // TODO(alemate): Find a more elegant solution.
  std::string sanitized_string = base::UTF16ToUTF8(raw_string);
  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, "\u00A0" /* NBSP */,
                                     "&nbsp;");
  return sanitized_string;
}

void SyncConsentScreenHandler::RememberLocalizedValue(
    const std::string& name,
    const int resource_id,
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add(name, resource_id);
  std::u16string raw_string = l10n_util::GetStringUTF16(resource_id);
  known_strings_[Sanitize(raw_string)] = resource_id;
}

void SyncConsentScreenHandler::RememberLocalizedValueWithDeviceName(
    const std::string& name,
    const int resource_id,
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF(name, resource_id, ui::GetChromeOSDeviceName());

  std::vector<std::u16string> str_substitute;
  str_substitute.push_back(ui::GetChromeOSDeviceName());
  std::u16string raw_string = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(resource_id), str_substitute, nullptr);
  known_strings_[Sanitize(raw_string)] = resource_id;
}

void SyncConsentScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  RememberLocalizedValueWithDeviceName(
      "syncConsentScreenTitle", IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE_WITH_DEVICE,
      builder);
  RememberLocalizedValue("syncConsentScreenSubtitle",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_SUBTITLE_2, builder);
  RememberLocalizedValueWithDeviceName(
      "syncConsentScreenOsSyncTitle",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_NAME_2, builder);
  RememberLocalizedValue(
      "syncConsentScreenChromeBrowserSyncTitle",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_NAME_2, builder);
  RememberLocalizedValue(
      "syncConsentScreenChromeBrowserSyncDescription",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_DESCRIPTION, builder);
  RememberLocalizedValue(
      "syncConsentReviewSyncOptionsText",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER, builder);

  RememberLocalizedValue("syncConsentAcceptAndContinue",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
                         builder);
  RememberLocalizedValue("syncConsentTurnOnSync",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_TURN_ON_SYNC, builder);

  // TODO(https://crbug.com/1278325): Remove this strings after removing
  //                                  the corresponding UI.
  RememberLocalizedValue("syncConsentScreenSplitSettingsTitle",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE, builder);
  RememberLocalizedValue("syncConsentScreenSplitSettingsSubtitle",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_SUBTITLE, builder);
  RememberLocalizedValue("syncConsentScreenOsSyncName",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_NAME, builder);
  RememberLocalizedValue("syncConsentScreenOsSyncDescription",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_DESCRIPTION,
                         builder);
  RememberLocalizedValue("syncConsentScreenChromeBrowserSyncName",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_NAME,
                         builder);
  RememberLocalizedValue("syncConsentScreenChromeSyncDescription",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
                         builder);
  RememberLocalizedValue(
      "syncConsentScreenPersonalizeGoogleServicesName",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME, builder);
  RememberLocalizedValue(
      "syncConsentScreenPersonalizeGoogleServicesDescriptionSupervisedUser",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION_SUPERVISED_USER,
      builder);
  RememberLocalizedValue(
      "syncConsentScreenPersonalizeGoogleServicesDescription",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
      builder);

  RememberLocalizedValue(
      "syncConsentScreenTitleArcRestrictions",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE_WITH_ARC_RESTRICTED, builder);
  RememberLocalizedValueWithDeviceName(
      "syncConsentScreenOsSyncDescriptionArcRestrictions",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_DESCRIPTION_WITH_ARC_RESTRICTED,
      builder);
  RememberLocalizedValue(
      "syncConsentReviewSyncOptionsWithArcRestrictedText",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER_ARC_RESTRICTED,
      builder);

  RememberLocalizedValue("syncConsentScreenAccept",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT2, builder);
  RememberLocalizedValue("syncConsentScreenDecline",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_DECLINE2, builder);
}

void SyncConsentScreenHandler::Bind(SyncConsentScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void SyncConsentScreenHandler::Show(bool is_arc_restricted) {
  auto* user_manager = user_manager::UserManager::Get();
  base::DictionaryValue data;
  data.SetBoolKey("isChildAccount", user_manager->IsLoggedInAsChildUser());
  data.SetBoolKey("isArcRestricted", is_arc_restricted);
  ShowScreenWithData(kScreenId, &data);
}

void SyncConsentScreenHandler::Hide() {}

void SyncConsentScreenHandler::SetThrobberVisible(bool visible) {
  CallJS("login.SyncConsentScreen.setThrobberVisible", visible);
}

void SyncConsentScreenHandler::SetIsMinorMode(bool value) {
  CallJS("login.SyncConsentScreen.setIsMinorMode", value);
}

void SyncConsentScreenHandler::Initialize() {}

void SyncConsentScreenHandler::RegisterMessages() {
  AddCallback("login.SyncConsentScreen.nonSplitSettingsContinue",
              &SyncConsentScreenHandler::HandleNonSplitSettingsContinue);
  AddCallback("login.SyncConsentScreen.acceptAndContinue",
              &SyncConsentScreenHandler::HandleAcceptAndContinue);
  AddCallback("login.SyncConsentScreen.declineAndContinue",
              &SyncConsentScreenHandler::HandleDeclineAndContinue);
}

void SyncConsentScreenHandler::HandleNonSplitSettingsContinue(
    const bool opted_in,
    const bool review_sync,
    const login::StringList& consent_description,
    const std::string& consent_confirmation) {
  std::vector<int> consent_description_ids;
  int consent_confirmation_id;
  GetConsentIDs(known_strings_, consent_description, consent_confirmation,
                &consent_description_ids, &consent_confirmation_id);
  screen_->OnNonSplitSettingsContinue(
      opted_in, review_sync, consent_description_ids, consent_confirmation_id);
  SyncConsentScreen::SyncConsentScreenTestDelegate* test_delegate =
      screen_->GetDelegateForTesting();
  if (test_delegate) {
    test_delegate->OnConsentRecordedStrings(consent_description,
                                            consent_confirmation);
  }
}

void SyncConsentScreenHandler::HandleAcceptAndContinue(
    const login::StringList& consent_description,
    const std::string& consent_confirmation) {
  NOTREACHED();
  // TODO(https://crbug.com/1278325): Remove this.
}

void SyncConsentScreenHandler::HandleDeclineAndContinue(
    const login::StringList& consent_description,
    const std::string& consent_confirmation) {
  NOTREACHED();
  // TODO(https://crbug.com/1278325): Remove this.
}

}  // namespace chromeos
