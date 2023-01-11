// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_UPGRADE_TYPES_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_UPGRADE_TYPES_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace offline_pages {

// Enumeration of possible results for starting the upgrade process.
enum class StartUpgradeStatus {
  SUCCESS,
  DB_ERROR,
  ITEM_MISSING,
  FILE_MISSING,
  NOT_ENOUGH_STORAGE,
};

// Result of starting the upgrade.
struct StartUpgradeResult {
  StartUpgradeResult();
  explicit StartUpgradeResult(StartUpgradeStatus status);
  StartUpgradeResult(StartUpgradeStatus status,
                     const std::string& digest,
                     const base::FilePath& file_path);

  // Support for move semantics.
  StartUpgradeResult(StartUpgradeResult&& other) = default;
  StartUpgradeResult& operator=(StartUpgradeResult&& other) = default;

  StartUpgradeStatus status;
  std::string digest;
  base::FilePath file_path;
};

// Callback delivering results of starting the upgrade.
typedef base::OnceCallback<void(StartUpgradeResult)> StartUpgradeCallback;

// Enumeration of possible statuses of upgrade process completion.
enum class CompleteUpgradeStatus {
  SUCCESS,
  DB_ERROR,
  ITEM_MISSING,
  DIGEST_VERIFICATION_FAILED,
  TEMPORARY_FILE_MISSING,
  TARGET_FILE_NAME_IN_USE,
  RENAMING_FAILED,
  DB_ERROR_POST_FILE_RENAME,
};

// Callback for completing the upgrade.
typedef base::OnceCallback<void(CompleteUpgradeStatus)> CompleteUpgradeCallback;

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_UPGRADE_TYPES_H_
