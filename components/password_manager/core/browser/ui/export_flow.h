// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_EXPORT_FLOW_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_EXPORT_FLOW_H_

#include "components/password_manager/core/browser/ui/export_progress_status.h"

namespace password_manager {

// This represents the controller for the UI flow of exporting passwords.
class ExportFlow {
 public:
  // Store exported passwords to the export destination. If an export is already
  // in progress this will do nothing and return false.
  virtual bool Store() = 0;

  // Cancel any previous Store() request and restore the state of the
  // filesystem. The cancellation request may come a few seconds after Store()
  // is completely finished.
  virtual void CancelStore() = 0;

  // Get the status of the export, which was initiated by Store().
  virtual password_manager::ExportProgressStatus GetExportProgressStatus() = 0;

 protected:
  virtual ~ExportFlow() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_EXPORT_FLOW_H_
