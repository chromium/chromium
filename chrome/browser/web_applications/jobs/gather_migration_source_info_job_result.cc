// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/gather_migration_source_info_job_result.h"

#include <utility>

#include "base/strings/to_string.h"

namespace web_app {

GatherMigrationSourceInfoJobResult::GatherMigrationSourceInfoJobResult() =
    default;
GatherMigrationSourceInfoJobResult::~GatherMigrationSourceInfoJobResult() =
    default;
GatherMigrationSourceInfoJobResult::GatherMigrationSourceInfoJobResult(
    const GatherMigrationSourceInfoJobResult&) = default;
GatherMigrationSourceInfoJobResult&
GatherMigrationSourceInfoJobResult::operator=(
    const GatherMigrationSourceInfoJobResult&) = default;
GatherMigrationSourceInfoJobResult::GatherMigrationSourceInfoJobResult(
    GatherMigrationSourceInfoJobResult&&) = default;
GatherMigrationSourceInfoJobResult&
GatherMigrationSourceInfoJobResult::operator=(
    GatherMigrationSourceInfoJobResult&&) = default;

base::Value GatherMigrationSourceInfoJobResult::ToDebugValue() const {
  base::DictValue debug_value;
  debug_value.Set("install_state", base::ToString(install_state));
  debug_value.Set("user_display_mode", base::ToString(user_display_mode));
  debug_value.Set("run_on_os_login_mode", base::ToString(run_on_os_login_mode));
  debug_value.Set("shortcut_locations", shortcut_locations.ToDebugValue());
  return base::Value(std::move(debug_value));
}

}  // namespace web_app
