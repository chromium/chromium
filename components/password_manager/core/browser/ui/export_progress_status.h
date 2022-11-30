// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_EXPORT_PROGRESS_STATUS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_EXPORT_PROGRESS_STATUS_H_

namespace password_manager {

enum class ExportProgressStatus {
  NOT_STARTED,
  IN_PROGRESS,
  SUCCEEDED,
  FAILED_CANCELLED,
  FAILED_WRITE_FAILED
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_EXPORT_PROGRESS_STATUS_H_
