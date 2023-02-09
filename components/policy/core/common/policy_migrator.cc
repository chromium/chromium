// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_migrator.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

void DoNothing(base::Value* val) {}

}  // namespace

PolicyMigrator::~PolicyMigrator() = default;

void PolicyMigrator::CopyPolicyIfUnset(PolicyMap& source,
                                       PolicyMap* dest,
                                       const Migration& migration) {
  PolicyMap::Entry* entry = source.GetMutable(migration.old_name);
  if (entry) {
    if (!dest->Get(migration.new_name)) {
      VLOG_POLICY(3, POLICY_PROCESSING)
          << "Legacy policy '" << migration.old_name << "' has been copied to '"
          << migration.new_name << "'.";
      auto new_entry = entry->DeepCopy();
      migration.transform.Run(new_entry.value_unsafe());
      new_entry.AddMessage(PolicyMap::MessageType::kWarning,
                           IDS_POLICY_MIGRATED_NEW_POLICY,
                           {base::UTF8ToUTF16(migration.old_name)});
      dest->Set(migration.new_name, std::move(new_entry));
    } else {
      VLOG_POLICY(3, POLICY_PROCESSING)
          << "Legacy policy '" << migration.old_name << "' is ignored because '"
          << migration.new_name << "' is also set. ";
    }
    entry->AddMessage(PolicyMap::MessageType::kError,
                      IDS_POLICY_MIGRATED_OLD_POLICY,
                      {base::UTF8ToUTF16(migration.new_name)});
  } else {
    VLOG_POLICY(3, POLICY_PROCESSING)
        << "Legacy policy '" << migration.old_name << "' is not set.";
  }
}

PolicyMigrator::Migration::Migration(Migration&&) = default;

PolicyMigrator::Migration::Migration(const char* old_name, const char* new_name)
    : Migration(old_name, new_name, base::BindRepeating(&DoNothing)) {}

PolicyMigrator::Migration::Migration(const char* old_name,
                                     const char* new_name,
                                     ValueTransform transform)
    : old_name(old_name), new_name(new_name), transform(std::move(transform)) {}

PolicyMigrator::Migration::~Migration() = default;

}  // namespace policy
