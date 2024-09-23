// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/cookie_clear_on_exit_migration_notice.h"

#include <functional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_host.h"

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

const void* const kCookieClearOnExitMigrationNoticeShowingUserDataKey =
    &kCookieClearOnExitMigrationNoticeShowingUserDataKey;

// User data indicating whether the notice is currently being shown.
class CookieClearOnExitMigrationNoticeShowingUserData
    : public base::SupportsUserData::Data {
 public:
  static bool HasForProfile(const Profile& profile) {
    return static_cast<CookieClearOnExitMigrationNoticeShowingUserData*>(
        profile.GetUserData(
            kCookieClearOnExitMigrationNoticeShowingUserDataKey));
  }

  static void CreateForProfile(Profile& profile) {
    CHECK(!HasForProfile(profile));
    profile.SetUserData(
        kCookieClearOnExitMigrationNoticeShowingUserDataKey,
        std::make_unique<CookieClearOnExitMigrationNoticeShowingUserData>());
  }

  static void RemoveForProfile(Profile& profile) {
    profile.RemoveUserData(kCookieClearOnExitMigrationNoticeShowingUserDataKey);
  }
};

void OpenCookieSettingsAndCloseDialog(Browser& browser,
                                      ui::DialogModel& model) {
  chrome::ShowSettingsSubPage(&browser, chrome::kOnDeviceSiteDataSubpage);
  model.host()->Close();
}

bool SetCookieClearOnExitMigrationComplete(PrefService& prefs, bool can_close) {
  prefs.SetBoolean(prefs::kCookieClearOnExitMigrationNoticeComplete, true);
  return can_close;
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

bool CanShowCookieClearOnExitMigrationNotice(const Browser& browser) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  Profile* profile = browser.profile();
  PrefService* prefs = profile->GetPrefs();

  if (prefs->GetBoolean(prefs::kCookieClearOnExitMigrationNoticeComplete)) {
    return false;
  }

  if (CookieClearOnExitMigrationNoticeShowingUserData::HasForProfile(
          *profile)) {
    return false;
  }

  if (!profile->IsRegularProfile()) {
    return false;
  }

  // User has to be signed in with UNO (non-syncing).
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!prefs->GetBoolean(prefs::kExplicitBrowserSignin)) {
    return false;
  }

  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return false;
  }

  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }

  return true;

#else
  return false;
#endif
}

void ShowCookieClearOnExitMigrationNotice(
    Browser& browser,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  CHECK(CanShowCookieClearOnExitMigrationNotice(browser));

  Profile& profile = *browser.profile();
  PrefService& prefs = *profile.GetPrefs();

  // Do not show if the dialog is already being shown for this profile.
  CookieClearOnExitMigrationNoticeShowingUserData::CreateForProfile(profile);

  // Marks the migration completes when the user interacts with the dialog.
  base::OnceCallback<void(bool)> set_migration_complete_callback =
      base::BindOnce(&SetCookieClearOnExitMigrationComplete, std::ref(prefs))
          .Then(std::move(callback));

  // Split the callback in 3: Ok, Cancel, Close.
  // Ok: Proceeds with closing the browser,
  // Cancel: Closes the dialog but keeps the browser open,
  // Close (e.g. by pressing ESC): same as cancel.
  auto [ok_callback, temp_callback] =
      base::SplitOnceCallback(std::move(set_migration_complete_callback));
  base::OnceClosure temp_cancel_closure =
      base::BindOnce(std::move(temp_callback), false);
  auto [cancel_closure, close_closure] =
      base::SplitOnceCallback(std::move(temp_cancel_closure));

  // Pressing the "go to settings" link closes the dialog, keeps the browser
  // open, and navigates to the cookie settings page.
  ui::DialogModel::Builder dialog_builder;
  ui::DialogModelLabel::TextReplacement link_replacement =
      ui::DialogModelLabel::CreateLink(
          IDS_CLOSE_WARNING_FOR_CLEAR_COOKIES_ON_EXIT_TEXT_LINK,
          base::BindRepeating(&OpenCookieSettingsAndCloseDialog,
                              std::ref(browser),
                              std::ref(*dialog_builder.model())));
  dialog_builder.SetInternalName("CookieClearOnExitMigrationNotice")
      .AddParagraph(ui::DialogModelLabel::CreateWithReplacement(
          IDS_CLOSE_WARNING_FOR_CLEAR_COOKIES_ON_EXIT_TEXT,
          std::move(link_replacement)))
      .AddOkButton(
          base::BindOnce(std::move(ok_callback), true),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_CLOSE_WARNING_FOR_CLEAR_COOKIES_ON_EXIT_CLOSE)))
      .AddCancelButton(std::move(cancel_closure),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_CANCEL)))
      .SetCloseActionCallback(std::move(close_closure))
      .SetDialogDestroyingCallback(base::BindOnce(
          &CookieClearOnExitMigrationNoticeShowingUserData::RemoveForProfile,
          std::ref(profile)));

  chrome::ShowBrowserModal(&browser, dialog_builder.Build());
#else
  NOTREACHED();
#endif
}
