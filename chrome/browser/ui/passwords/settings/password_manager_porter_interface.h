// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_INTERFACE_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_INTERFACE_H_

#include "components/password_manager/core/browser/export/export_progress_status.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/password_form.h"

namespace content {
class WebContents;
}

// Interface for PasswordManagerPorter to allow unittesting methods that use it.
class PasswordManagerPorterInterface {
 public:
  using ImportResultsCallback =
      base::OnceCallback<void(const password_manager::ImportResults&)>;

  PasswordManagerPorterInterface() = default;
  virtual ~PasswordManagerPorterInterface() = default;

  // Triggers passwords export flow for the given |web_contents|.
  virtual bool Export(base::WeakPtr<content::WebContents> web_contents) = 0;

  virtual void CancelExport() = 0;

  virtual password_manager::ExportProgressStatus GetExportProgressStatus() = 0;

  // Triggers passwords import flow for the given |web_contents|.
  // Passwords will be imported into the |to_store|.
  // |results_callback| is used to return import summary back to the user. It is
  // run on the completion of import flow.
  virtual void Import(content::WebContents* web_contents,
                      password_manager::PasswordForm::Store to_store,
                      ImportResultsCallback results_callback) = 0;

  // Resumes the password import process when user has selected which passwords
  // to replace. The caller earlier received an array with conflicting
  // ImportEntry's that are displayed to the user, the ids of the selected items
  // correspond to indices of conflicting credentials in PasswordImporter.
  // |selected_ids|: The ids of passwords that need to be replaced.
  // |results_callback|: It is used to return import summary back to the user.
  virtual void ContinueImport(const std::vector<int>& selected_ids,
                              ImportResultsCallback results_callback) = 0;

  // Resets the PasswordImporter if it is in the CONFLICTS/FINISHED. Only when
  // the PasswordImporter is in FINISHED state, |deleteFile| option is taken
  // into account.
  // |deleteFile|: Whether to trigger deletion of the last imported file.
  virtual void ResetImporter(bool delete_file) = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_INTERFACE_H_
