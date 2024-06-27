// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREFERENCES_MERGE_HELPER_H_
#define COMPONENTS_SYNC_PREFERENCES_PREFERENCES_MERGE_HELPER_H_

#include <string_view>
#include <utility>

#include "base/values.h"

namespace sync_preferences {

class PrefModelAssociatorClient;

namespace helper {

// Merges `local_value` and `server_value` list values.
// All entries of `server_value` come first and then of `local_value`. Any
// repeating value in `local_value` is excluded in the result.
base::Value::List MergeListValues(const base::Value::List& local_value,
                                  const base::Value::List& server_value);

// Merges `local_value` and `server_value` dict values.
// Entry from `server_value` wins in case of conflict.
base::Value::Dict MergeDictionaryValues(const base::Value::Dict& local_value,
                                        const base::Value::Dict& server_value);

// Merges the `local_value` into the supplied `server_value` and returns
// the result. If there is a conflict, the server value takes precedence. Note
// that only certain preferences will actually be merged, all others will
// return a copy of the server value.
// Note: `client` can NULL in some tests, in which case `server_value` is
// returned.
base::Value MergePreference(const PrefModelAssociatorClient* client,
                            std::string_view pref_name,
                            const base::Value& local_value,
                            const base::Value& server_value);

// This separates individual dictionary pref updates between the account store
// and the local store, from the updated merged value `new_value`. Returns a
// pair with the first item being the updated local value, followed by the
// updated account value.
std::pair<base::Value::Dict, base::Value::Dict> UnmergeDictionaryValues(
    base::Value::Dict new_value,
    const base::Value::Dict& original_local_value,
    const base::Value::Dict& original_account_value);

}  // namespace helper
}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREFERENCES_MERGE_HELPER_H_
