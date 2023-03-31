// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREFERENCES_MERGE_HELPER_H_
#define COMPONENTS_SYNC_PREFERENCES_PREFERENCES_MERGE_HELPER_H_

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
base::Value MergePreference(const PrefModelAssociatorClient* client,
                            const std::string& pref_name,
                            const base::Value& local_value,
                            const base::Value& server_value);

}  // namespace helper
}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREFERENCES_MERGE_HELPER_H_
