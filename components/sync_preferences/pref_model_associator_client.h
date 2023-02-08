// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_CLIENT_H_
#define COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_CLIENT_H_

#include <string>

#include "base/values.h"

namespace sync_preferences {

class SyncablePrefsDatabase;

// This class allows the embedder to configure the PrefModelAssociator to
// have a different behaviour when receiving preference synchronisations
// events from the server.
class PrefModelAssociatorClient {
 public:
  PrefModelAssociatorClient(const PrefModelAssociatorClient&) = delete;
  PrefModelAssociatorClient& operator=(const PrefModelAssociatorClient&) =
      delete;

  // Returns true if the preference named |pref_name| is a list preference
  // whose server value is merged with local value during synchronisation.
  virtual bool IsMergeableListPreference(
      const std::string& pref_name) const = 0;

  // Returns true if the preference named |pref_name| is a dictionary preference
  // whose server value is merged with local value during synchronisation.
  virtual bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const = 0;

  // Returns the merged value if the client wants to apply a custom merging
  // strategy to the preference named |pref_name| with local value |local_value|
  // and server-provided value |server_value|. Otherwise, returns |nullptr| and
  // the server's value will be chosen.
  virtual base::Value MaybeMergePreferenceValues(
      const std::string& pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const = 0;

  // Returns a pointer to the instance of SyncablePrefsDatabase. This should
  // define the list of syncable preferences.
  // TODO(crbug.com/1401271): Mark this method as pure virtual once
  // platform-specific implementations are complete.
  virtual const SyncablePrefsDatabase& GetSyncablePrefsDatabase() const;

 protected:
  PrefModelAssociatorClient() {}
  virtual ~PrefModelAssociatorClient() {}
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_CLIENT_H_
