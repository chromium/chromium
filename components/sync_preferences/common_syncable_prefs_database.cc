// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/common_syncable_prefs_database.h"

#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_piece.h"

namespace sync_preferences {
namespace {
// List of syncable preferences common across platforms.
constexpr auto kCommonSyncablePrefsAllowlist =
    base::MakeFixedFlatSet<base::StringPiece>({"dummy"});
}  // namespace

bool CommonSyncablePrefsDatabase::IsPreferenceSyncable(
    const std::string& pref_name) const {
  return kCommonSyncablePrefsAllowlist.count(pref_name);
}

}  // namespace sync_preferences
