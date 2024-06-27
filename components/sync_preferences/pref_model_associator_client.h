// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_CLIENT_H_
#define COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_CLIENT_H_

#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/values.h"

namespace sync_preferences {

class SyncablePrefsDatabase;

// This class allows the embedder to configure the PrefModelAssociator to
// have a different behaviour when receiving preference synchronisations
// events from the server.
class PrefModelAssociatorClient
    : public base::RefCounted<PrefModelAssociatorClient> {
 public:
  PrefModelAssociatorClient(const PrefModelAssociatorClient&) = delete;
  PrefModelAssociatorClient& operator=(const PrefModelAssociatorClient&) =
      delete;

  // Returns the merged value if the client wants to apply a custom merging
  // strategy to the preference named |pref_name| with local value |local_value|
  // and server-provided value |server_value|. Otherwise, returns |nullptr| and
  // the server's value will be chosen.
  virtual base::Value MaybeMergePreferenceValues(
      std::string_view pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const = 0;

  // Returns a pointer to the instance of SyncablePrefsDatabase. This should
  // define the list of syncable preferences.
  virtual const SyncablePrefsDatabase& GetSyncablePrefsDatabase() const = 0;

 protected:
  friend class base::RefCounted<PrefModelAssociatorClient>;
  PrefModelAssociatorClient() = default;
  virtual ~PrefModelAssociatorClient() = default;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_CLIENT_H_
