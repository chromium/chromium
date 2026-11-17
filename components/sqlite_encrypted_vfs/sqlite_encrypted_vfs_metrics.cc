// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace sqlite_encrypted_vfs {

// LINT.IfChange(EncryptedVfsDatabaseAction)
std::string_view DatabaseActionToString(DatabaseAction action) {
  switch (action) {
    case DatabaseAction::kOpen:
      return "Open";
    case DatabaseAction::kDelete:
      return "Delete";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/sql/histograms.xml:EncryptedVfsDatabaseAction)

// LINT.IfChange(EncryptedVfsKeyAction)
std::string_view KeyActionToString(KeyAction action) {
  switch (action) {
    case KeyAction::kCreate:
      return "Create";
    case KeyAction::kLoad:
      return "Load";
    case KeyAction::kReseal:
      return "Reseal";
    case KeyAction::kDelete:
      return "Delete";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/sql/histograms.xml:EncryptedVfsKeyAction)

std::string GetDatabaseActionHistogramName(DatabaseAction action) {
  return base::StrCat(
      {"Sql.EncryptedVfs.Database.", DatabaseActionToString(action)});
}

std::string GetDatabaseActionTimeHistogramName(DatabaseAction action) {
  return base::StrCat({GetDatabaseActionHistogramName(action), ".Time"});
}

std::string GetKeyActionHistogramName(KeyAction action) {
  return base::StrCat({"Sql.EncryptedVfs.Key.", KeyActionToString(action)});
}

std::string GetKeyActionTimeHistogramName(KeyAction action) {
  return base::StrCat({GetKeyActionHistogramName(action), ".Time"});
}

void RecordDatabaseAction(DatabaseAction action,
                          bool success,
                          base::TimeDelta elapsed) {
  base::UmaHistogramBoolean(GetDatabaseActionHistogramName(action), success);
  base::UmaHistogramTimes(GetDatabaseActionTimeHistogramName(action), elapsed);
}

void RecordKeyAction(KeyAction action, bool success, base::TimeDelta elapsed) {
  base::UmaHistogramBoolean(GetKeyActionHistogramName(action), success);
  base::UmaHistogramTimes(GetKeyActionTimeHistogramName(action), elapsed);
}

}  // namespace sqlite_encrypted_vfs
