// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAVED_INFO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAVED_INFO_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class Profile;

namespace settings {

class SavedInfoHandler
    : public SettingsPageUIHandler,
      public password_manager::SavedPasswordsPresenter::Observer {
 public:
  explicit SavedInfoHandler(Profile* profile);
  explicit SavedInfoHandler(
      Profile* profile,
      std::unique_ptr<password_manager::SavedPasswordsPresenter> presenter);
  ~SavedInfoHandler() override;

  SavedInfoHandler(const SavedInfoHandler&) = delete;
  SavedInfoHandler& operator=(const SavedInfoHandler&) = delete;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class TestSavedInfoHandler;
  FRIEND_TEST_ALL_PREFIXES(SavedInfoHandlerTest, HandleGetPasswordCount);

  // password_manager::SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  void HandleGetPasswordCount(const base::Value::List& args);
  base::Value::Dict GetPasswordCounts();

  raw_ptr<Profile> profile_;

  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      saved_passwords_presenter_;

  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      observation_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAVED_INFO_HANDLER_H_
