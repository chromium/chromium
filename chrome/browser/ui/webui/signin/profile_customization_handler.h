// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"

namespace base {
class ListValue;
}

class ProfileAttributesEntry;

// WebUI message handler for the profile customization bubble.
class ProfileCustomizationHandler : public content::WebUIMessageHandler,
                                    public ProfileAttributesStorage::Observer {
 public:
  explicit ProfileCustomizationHandler(base::OnceClosure done_closure);
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
  // Handlers for messages from javascript.
  void HandleInitialized(const base::ListValue* args);
  void HandleDone(const base::ListValue* args);

  // Sends an updated profile info (avatar and colors) to the WebUI.
  // `profile_path` is the path of the profile being updated, this function does
  // nothing if the profile path does not match the current profile.
  void UpdateProfileInfo(const base::FilePath& profile_path);

  // Computes the profile info (avatar and colors) to be sent to the WebUI.
  base::Value GetProfileInfoValue();

  // Returns the ProfilesAttributesEntry associated with the current profile.
  ProfileAttributesEntry* GetProfileEntry() const;

  base::FilePath profile_path_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      observed_profile_{this};

  // Called when the "Done" button has been pressed.
  base::OnceClosure done_closure_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_HANDLER_H_
