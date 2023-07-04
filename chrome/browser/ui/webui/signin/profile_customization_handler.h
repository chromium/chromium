// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"

class Profile;
class ProfileAttributesEntry;

// WebUI message handler for the profile customization bubble.
class ProfileCustomizationHandler : public content::WebUIMessageHandler,
                                    public ProfileAttributesStorage::Observer {
 public:
  enum class CustomizationResult {
    // User clicked on the "Done" button.
    kDone = 0,
    // User clicked on the "Skip" button.
    kSkip = 1,
  };

  explicit ProfileCustomizationHandler(
      Profile* profile,
      base::OnceCallback<void(CustomizationResult)> completion_callback);
  ~ProfileCustomizationHandler() override;

  ProfileCustomizationHandler(const ProfileCustomizationHandler&) = delete;
  ProfileCustomizationHandler& operator=(const ProfileCustomizationHandler&) =
      delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileThemeColorsChanged(const base::FilePath& profile_path) override;
  void OnProfileHostedDomainChanged(
      const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;

 private:
  friend class ProfilePickerCreationFlowBrowserTest;

  // Handlers for messages from javascript.
  void HandleInitialized(const base::Value::List& args);
  void HandleGetAvailableIcons(const base::Value::List& args);
  void HandleDone(const base::Value::List& args);
  void HandleSkip(const base::Value::List& args);
  void HandleDeleteProfile(const base::Value::List& args);
  void HandleSetAvatarIcon(const base::Value::List& args);

  // Sends an updated profile info (avatar and colors) to the WebUI.
  // `profile_path` is the path of the profile being updated, this function does
  // nothing if the profile path does not match the current profile.
  void UpdateProfileInfo(const base::FilePath& profile_path);

  // Computes the profile info (avatar and colors) to be sent to the WebUI.
  base::Value::Dict GetProfileInfoValue();

  // Returns the ProfilesAttributesEntry associated with the current profile.
  ProfileAttributesEntry* GetProfileEntry() const;

  // Non-owning pointer to the associated profile.
  raw_ptr<Profile> profile_;

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      observed_profile_{this};

  // Called when the "Done" or "Skip" button has been clicked. The callback
  // normally closes native widget hosting Profile Customization webUI.
  base::OnceCallback<void(CustomizationResult)> completion_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_HANDLER_H_
