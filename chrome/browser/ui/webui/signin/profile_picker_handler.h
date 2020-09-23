// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_

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
  void HandleMainViewInitialize(const base::ListValue* args);
  void HandleLaunchSelectedProfile(bool open_settings,
                                   const base::ListValue* args);
  void HandleLaunchGuestProfile(const base::ListValue* args);
  void HandleAskOnStartupChanged(const base::ListValue* args);
  void HandleRemoveProfile(const base::ListValue* args);
  void HandleGetProfileStatistics(const base::ListValue* args);

  // TODO(crbug.com/1115056): Move to new handler for profile creation.
  void HandleLoadSignInProfileCreationFlow(const base::ListValue* args);
  void HandleGetNewProfileSuggestedThemeInfo(const base::ListValue* args);
  void HandleGetProfileThemeInfo(const base::ListValue* args);
  void HandleCreateProfile(const base::ListValue* args);

  void OnLoadSigninFailed();
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

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Creation time of the handler, to measure performance on startup. Only set
  // when the picker is shown on startup.
  base::TimeTicks creation_time_on_startup_;
  bool main_view_initialized_ = false;

  base::WeakPtrFactory<ProfilePickerHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfilePickerHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_
