// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_EXPORT_PROGRESS_STATUS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_EXPORT_PROGRESS_STATUS_H_

namespace password_manager {

enum class ExportProgressStatus {
  kNotStarted,
  kInProgress,
  kSucceeded,
  kFailedCancelled,
  kFailedWrite
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_EXPORT_PROGRESS_STATUS_H_
