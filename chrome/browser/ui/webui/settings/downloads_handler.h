// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_DOWNLOADS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_DOWNLOADS_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace settings {

// Chrome "Downloads" settings page UI handler.
class DownloadsHandler : public SettingsPageUIHandler,
                         public ui::SelectFileDialog::Listener {
 public:
  explicit DownloadsHandler(Profile* profile);

  DownloadsHandler(const DownloadsHandler&) = delete;
  DownloadsHandler& operator=(const DownloadsHandler&) = delete;

  ~DownloadsHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class DownloadsHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(DownloadsHandlerTest, AutoOpenDownloads);

  // Callback for the "initializeDownloads" message. This starts observers and
  // retrieves the current browser state.
  void HandleInitialize(const base::Value::List& args);

  void SendAutoOpenDownloadsToJavascript();

  // Resets the list of filetypes that are auto-opened after download.
  void HandleResetAutoOpenFileTypes(const base::Value::List& args);

  // Callback for the "selectDownloadLocation" message. This will prompt the
  // user for a destination folder using platform-specific APIs.
  void HandleSelectDownloadLocation(const base::Value::List& args);

  // SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Callback for the "getDownloadLocationText" message.  Converts actual
  // paths in chromeos to values suitable to display to users.
  // E.g. /home/chronos/u-<hash>/MyFiles/Downloads => "My Files > Downloads".
  void HandleGetDownloadLocationText(const base::Value::List& args);
#endif

  raw_ptr<Profile> profile_;

  PrefChangeRegistrar pref_registrar_;

  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_DOWNLOADS_HANDLER_H_
