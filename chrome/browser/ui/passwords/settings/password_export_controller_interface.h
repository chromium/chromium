// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_EXPORT_CONTROLLER_INTERFACE_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_EXPORT_CONTROLLER_INTERFACE_H_

#include "components/password_manager/core/browser/export/export_progress_status.h"

namespace content {
class WebContents;
}

// Interface for PasswordExportController to allow unittesting methods that use
// it.
class PasswordExportControllerInterface {
 public:
  PasswordExportControllerInterface() = default;
  virtual ~PasswordExportControllerInterface() = default;

  // Triggers passwords export flow for the given |web_contents|.
  virtual bool Export(content::WebContents* web_contents) = 0;

  virtual void CancelExport() = 0;

  virtual password_manager::ExportProgressStatus GetExportProgressStatus() = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_EXPORT_CONTROLLER_INTERFACE_H_
