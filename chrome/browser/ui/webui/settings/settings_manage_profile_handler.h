// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MANAGE_PROFILE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MANAGE_PROFILE_HANDLER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace settings {

// Chrome personal stuff profiles manage overlay UI handler.
class ManageProfileHandler : public settings::SettingsPageUIHandler,
                             public ProfileAttributesStorage::Observer {
 public:
  explicit ManageProfileHandler(Profile* profile);

  ManageProfileHandler(const ManageProfileHandler&) = delete;
  ManageProfileHandler& operator=(const ManageProfileHandler&) = delete;

  ~ManageProfileHandler() override;

  // settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileThemeColorsChanged(const base::FilePath& profile_path) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest,
                           HandleSetProfileIconToGaiaAvatar);
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest,
                           GetAvailableIconsSignedInProfile);
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest,
                           GetAvailableIconsLocalProfile);
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest,
                           HandleSetProfileIconToDefaultCustomAvatar);
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest,
                           HandleSetProfileIconToDefaultGenericAvatar);
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest, HandleSetProfileName);
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest, HandleGetAvailableIcons);
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest,
                           HandleGetAvailableIconsOldIconSelected);

  // Callback for the "getAvailableIcons" message.
  // Sends the array of default profile icon URLs and profile names to WebUI.
  void HandleGetAvailableIcons(const base::Value::List& args);

  // Callback for the "setProfileIconToGaiaAvatar" message.
  void HandleSetProfileIconToGaiaAvatar(const base::Value::List& args);

  // Callback for the "setProfileIconToDefaultAvatar" message.
  void HandleSetProfileIconToDefaultAvatar(const base::Value::List& args);

  // Callback for the "setProfileName" message.
  void HandleSetProfileName(const base::Value::List& args);

  // Callback for the "requestProfileShortcutStatus" message, which is called
  // when editing an existing profile. Asks the profile shortcut manager whether
  // the profile has shortcuts and gets the result in |OnHasProfileShortcuts()|.
  void HandleRequestProfileShortcutStatus(const base::Value::List& args);

  // Callback invoked from the profile manager indicating whether the profile
  // being edited has any desktop shortcuts.
  void OnHasProfileShortcuts(const std::string& callback_id,
                             bool has_shortcuts);

  // Callback for the "addProfileShortcut" message, which is called when editing
  // an existing profile and the user clicks the "Add desktop shortcut" button.
  // Adds a desktop shortcut for the profile.
  void HandleAddProfileShortcut(const base::Value::List& args);

  // Callback for the "removeProfileShortcut" message, which is called when
  // editing an existing profile and the user clicks the "Remove desktop
  // shortcut" button. Removes the desktop shortcut for the profile.
  void HandleRemoveProfileShortcut(const base::Value::List& args);

  // Non-owning pointer to the associated profile.
  raw_ptr<Profile> profile_;

  // Used to observe profile avatar updates.
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      observation_{this};

  // For generating weak pointers to itself for callbacks.
  base::WeakPtrFactory<ManageProfileHandler> weak_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MANAGE_PROFILE_HANDLER_H_
