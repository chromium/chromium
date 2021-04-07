// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_

#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler for Javascript messages related to the profile picker main view.
class ProfilePickerHandler : public content::WebUIMessageHandler,
                             public content::WebContentsObserver,
                             public ProfileAttributesStorage::Observer {
 public:
  ProfilePickerHandler();
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
  friend class ProfilePickerCreationFlowBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(
      ProfilePickerIntegratedEnterpriseCreationFlowBrowserTest,
      CreateSignedInProfileSigninAlreadyExists_ConfirmSwitch);
  FRIEND_TEST_ALL_PREFIXES(
      ProfilePickerIntegratedEnterpriseCreationFlowBrowserTest,
      CreateSignedInProfileSigninAlreadyExists_CancelSwitch);

  void HandleMainViewInitialize(const base::ListValue* args);
  void HandleLaunchSelectedProfile(bool open_settings,
                                   const base::ListValue* args);
  void HandleLaunchGuestProfile(const base::ListValue* args);
  void HandleAskOnStartupChanged(const base::ListValue* args);
  void HandleRemoveProfile(const base::ListValue* args);
  void HandleGetProfileStatistics(const base::ListValue* args);
  void HandleSetProfileName(const base::ListValue* args);

  // TODO(crbug.com/1115056): Move to new handler for profile creation.
  void HandleLoadSignInProfileCreationFlow(const base::ListValue* args);
  void HandleGetNewProfileSuggestedThemeInfo(const base::ListValue* args);
  void HandleGetProfileThemeInfo(const base::ListValue* args);
  void HandleGetAvailableIcons(const base::ListValue* args);
  void HandleCreateProfile(const base::ListValue* args);

  // Profile switch screen:
  void HandleGetSwitchProfile(const base::ListValue* args);
  void HandleConfirmProfileSwitch(const base::ListValue* args);
  void HandleCancelProfileSwitch(const base::ListValue* args);

  // |args| is unused.
  void HandleRecordSignInPromoImpression(const base::ListValue* args);

  void OnLoadSigninFinished(bool success);
  void GatherProfileStatistics(Profile* profile);
  void OnProfileStatisticsReceived(base::FilePath profile_path,
                                   profiles::ProfileCategoryStats result);
  void OnSwitchToProfileComplete(bool new_profile,
                                 bool open_settings,
                                 Profile* profile,
                                 Profile::CreateStatus profile_create_status);
  void OnProfileCreated(base::Optional<SkColor> profile_color,
                        bool create_shortcut,
                        Profile* profile,
                        Profile::CreateStatus status);
  void OnProfileCreationSuccess(base::Optional<SkColor> profile_color,
                                bool create_shortcut,
                                Profile* profile);
  void PushProfilesList();
  base::Value GetProfilesList();
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

  // Returns the list of profiles in the same order as when the picker
  // was first shown. Guest profile is not included here.
  std::vector<ProfileAttributesEntry*> GetProfileAttributes();

  // Creation time of the handler, to measure performance on startup. Only set
  // when the picker is shown on startup.
  base::TimeTicks creation_time_on_startup_;
  bool main_view_initialized_ = false;

  // The order of the profiles when the picker was first shown. This is used
  // to freeze the order of profiles on the picker. Newly added profiles, will
  // be added to the end of the list.
  std::unordered_map<base::FilePath, size_t> profiles_order_;
  base::WeakPtrFactory<ProfilePickerHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfilePickerHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_
