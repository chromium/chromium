// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_

#include <unordered_map>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "chrome/browser/lacros/account_manager/get_account_information_helper.h"
#include "chrome/browser/lacros/identity_manager_lacros.h"
#include "chrome/browser/ui/webui/signin/profile_picker_lacros_sign_in_provider.h"

class ProfilePickerLacrosSignInProvider;

namespace account_manager {
struct Account;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// The handler for Javascript messages related to the profile picker main view.
class ProfilePickerHandler : public content::WebUIMessageHandler,
                             public content::WebContentsObserver,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
                             public AccountProfileMapper::Observer,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
                             public ProfileAttributesStorage::Observer {
 public:
  ProfilePickerHandler();

  ProfilePickerHandler(const ProfilePickerHandler&) = delete;
  ProfilePickerHandler& operator=(const ProfilePickerHandler&) = delete;

  ~ProfilePickerHandler() override;

  // Enables the startup performance metrics. Should only be called when the
  // profile picker is shown on startup.
  void EnableStartupMetrics();

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class ProfilePickerHandlerTest;
  friend class ProfilePickerHandlerInUserProfileTest;
  friend class ProfilePickerCreationFlowBrowserTest;
  friend class ProfilePickerEnterpriseCreationFlowBrowserTest;
  friend class ProfilePickerLocalProfileCreationDialogBrowserTest;
  friend class StartupBrowserCreatorPickerInfobarTest;
  FRIEND_TEST_ALL_PREFIXES(ProfilePickerHandlerInUserProfileTest,
                           HandleExtendedAccountInformation);
  FRIEND_TEST_ALL_PREFIXES(ProfilePickerCreationFlowBrowserTest,
                           CloseBrowserBeforeCreatingNewProfile);
  FRIEND_TEST_ALL_PREFIXES(
      ProfilePickerEnterpriseCreationFlowBrowserTest,
      CreateSignedInProfileSigninAlreadyExists_ConfirmSwitch);
  FRIEND_TEST_ALL_PREFIXES(
      ProfilePickerEnterpriseCreationFlowBrowserTest,
      CreateSignedInProfileSigninAlreadyExists_CancelSwitch);

  void HandleMainViewInitialize(const base::Value::List& args);
  void HandleLaunchSelectedProfile(bool open_settings,
                                   const base::Value::List& args);
  void HandleLaunchGuestProfile(const base::Value::List& args);
  void HandleAskOnStartupChanged(const base::Value::List& args);
  void HandleRemoveProfile(const base::Value::List& args);
  void HandleGetProfileStatistics(const base::Value::List& args);
  void HandleSetProfileName(const base::Value::List& args);

  void HandleSelectNewAccount(const base::Value::List& args);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Add an existing account to a Profile.
  void HandleSelectExistingAccountLacros(const base::Value::List& args);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  void HandleGetNewProfileSuggestedThemeInfo(const base::Value::List& args);
  void HandleGetProfileThemeInfo(const base::Value::List& args);
  void HandleGetAvailableIcons(const base::Value::List& args);
  void HandleCreateProfile(const base::Value::List& args);
  // This function creates a new local profile and opens the profile
  // customization in a modal dialog.
  void HandleCreateProfileAndOpenCustomizationDialog(
      const base::Value::List& args);

  // Profile switch screen:
  void HandleGetSwitchProfile(const base::Value::List& args);
  void HandleConfirmProfileSwitch(const base::Value::List& args);
  void HandleCancelProfileSwitch(const base::Value::List& args);

  // |args| is unused.
  void HandleRecordSignInPromoImpression(const base::Value::List& args);

  void OnLoadSigninFinished(bool success);
  void GatherProfileStatistics(Profile* profile);
  void OnProfileStatisticsReceived(const base::FilePath& profile_path,
                                   profiles::ProfileCategoryStats result);
  void OnSwitchToProfileComplete(bool new_profile,
                                 bool open_settings,
                                 Profile* profile);
  void OnSwitchToProfileCompleteOpenCustomization(Profile* profile);
  void OnProfileInitialized(absl::optional<SkColor> profile_color,
                            bool create_shortcut,
                            Profile* profile);
  void OnLocalProfileInitialized(absl::optional<SkColor> profile_color,
                                 Profile* profile);
  void PushProfilesList();
  base::Value::List GetProfilesList();
  // Adds a profile with `profile_path` to `profiles_order_`.
  void AddProfileToList(const base::FilePath& profile_path);
  // Removes a profile with `profile_path` from `profiles_order_`. Returns
  // true if the profile was found and removed. Otherwise, returns false.
  bool RemoveProfileFromList(const base::FilePath& profile_path);

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;
  void OnProfileIsOmittedChanged(const base::FilePath& profile_path) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;
  void OnProfileHostedDomainChanged(
      const base::FilePath& profile_path) override;

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Sets 'profiles_order_' that is used to freeze the order of the profiles on
  // the picker when it was first shown.
  void SetProfilesOrder(const std::vector<ProfileAttributesEntry*>& entries);

  // Displays either a sign-in or an error dialog within the profile picker
  // using `profile`.
  void OnProfileForDialogLoaded(Profile* profile);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // GaiaId as input string.
  using AddAccountCallback = base::OnceCallback<void(const std::string&)>;

  // Opens the Ash account settings page in a new window.
  void HandleOpenAshAccountSettingsPage(const base::Value::List& args);

  // List of available accounts used by the profile choice and the account
  // selection screens.
  void HandleGetAvailableAccounts(const base::Value::List& args);

  // Queries accounts available for addition in the profile, and ends up sending
  // them to the WebUI page.
  void UpdateAvailableAccounts();

  // Loads extended info for accounts from Ash.
  void GetAvailableAccountsInfo(
      const std::vector<account_manager::Account>& accounts);
  // Sends extended info for accounts to the WebUI page.
  void SendAvailableAccounts(
      std::vector<GetAccountInformationHelper::GetAccountInformationResult>
          accounts);

  // Called when a new Lacros signed-in profile is created. The profile is
  // omitted, ephemeral, and has a primary kSignin account.
  void OnLacrosSignedInProfileCreated(absl::optional<SkColor> profile_color,
                                      Profile* profile);

  // `AddAccountCallback` used with `ResultAccountInPersistentError`.
  void AddExistingAccountToNewProfile(
      ProfilePickerLacrosSignInProvider::SignedInCallback signed_in_callback,
      const std::string& gaia_id);

  // `AddAccountCallback` used with `ResultAccountInPersistentError`.
  void AddExistingAccountToExistingProfile(
      AccountProfileMapper* mapper,
      AccountProfileMapper::AddAccountCallback add_account_callback,
      const std::string& gaia_id);

  // Callback result of IdentityManagerLacros to check if the input account is
  // in persistent error. Shows reauth screen on account in error, or calls
  // `add_account_callback` on success.
  // - `gaia_id`: id of account to check.
  // - `add_account_callback`: callback on success.
  // - `source`: Reauth screen source.
  // - `persistent_error`: account check result.
  void ResultAccountInPersistentError(
      const std::string& gaia_id,
      AddAccountCallback add_account_callback,
      account_manager::AccountManagerFacade::AccountAdditionSource source,
      bool persistent_error);

  // Displays the reauth screen with the given email and source.
  void ShowReauthWithEmail(
      account_manager::AccountManagerFacade::AccountAdditionSource source,
      const std::string& email);

  // Resets account_selected state through JS to allow another account to be
  // selected, also resets `lacros_sign_in_provider_`.
  void OnReauthDialogClosed();

  // AccountProfileMapper::Observer:
  void OnAccountUpserted(const base::FilePath& profile_path,
                         const account_manager::Account& account) override;
  void OnAccountRemoved(const base::FilePath& profile_path,
                        const account_manager::Account& account) override;

  // Adds an existing account (if gaia_id has a value) or initiates the
  // signin flow to add a new account in a new/existing profile.
  void SelectAccountLacrosInternal(const std::string& gaia_id,
                                   absl::optional<SkColor> profile_color);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Returns the list of profiles in the same order as when the picker
  // was first shown.
  std::vector<ProfileAttributesEntry*> GetProfileAttributes();

  // Observes changes to profile attributes, and notifies the WebUI.
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_attributes_storage_observation_{this};

  // Creation time of the handler, to measure performance on startup. Only set
  // when the picker is shown on startup.
  base::TimeTicks creation_time_on_startup_;

  // Time when the user picked a profile to open, to measure browser startup
  // performance. Only set when the picker is shown on startup.
  base::TimeTicks profile_picked_time_on_startup_;

  bool main_view_initialized_ = false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Takes care of getting a signed-in profile.
  std::unique_ptr<ProfilePickerLacrosSignInProvider> lacros_sign_in_provider_;

  // Retrieves extended info for available accounts from Ash.
  std::unique_ptr<GetAccountInformationHelper> lacros_account_info_helper_;

  // Observes AccountProfileMapper to react to changes in available accounts.
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      account_profile_mapper_observation_{this};

  std::unique_ptr<IdentityManagerLacros> identity_manager_lacros_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // The order of the profiles when the picker was first shown. This is used
  // to freeze the order of profiles on the picker. Newly added profiles, will
  // be added to the end of the list.
  std::unordered_map<base::FilePath, size_t> profiles_order_;
  base::WeakPtrFactory<ProfilePickerHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_
