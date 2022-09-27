// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_INTERFACE_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_INTERFACE_H_

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/export_progress_status.h"
#include "components/password_manager/core/browser/ui/import_results.h"

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
  virtual bool Export(content::WebContents* web_contents) = 0;

  virtual void CancelExport() = 0;

  virtual password_manager::ExportProgressStatus GetExportProgressStatus() = 0;

  // Triggers passwords import flow for the given |web_contents|.
  // Passwords will be imported into the |to_store|.
  // |results_callback| is used to return import summary back to the user. It is
  // run on the completion of import flow.
  virtual void Import(content::WebContents* web_contents,
                      password_manager::PasswordForm::Store to_store,
                      ImportResultsCallback results_callback) = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PORTER_INTERFACE_H_
