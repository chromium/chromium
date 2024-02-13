// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PREFS_DELEGATE_H_
#define COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PREFS_DELEGATE_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/metrics/structured/lib/key_data.h"
#include "components/prefs/pref_service.h"

namespace metrics::structured {

class KeyDataPrefsDelegateTest;
FORWARD_DECLARE_TEST(KeyDataPrefsDelegateTest, Purge);

// Storages Structured Metrics key data in the devices preferences.
//
// The keys are stored as a dictionary keyed by the project name and the value
// is a base::Value representation of KeyProto.
//
// Note: users are responsible for registering the preference.
class KeyDataPrefsDelegate : public KeyData::StorageDelegate {
 public:
  KeyDataPrefsDelegate(PrefService* local_state, std::string_view pref_name);

  ~KeyDataPrefsDelegate() override;

  // KeyData::StorageDelegate:
  bool IsReady() const override;
  const KeyProto* GetKey(uint64_t project_name_hash) const override;
  void UpsertKey(uint64_t project_name_hash,
                 base::TimeDelta last_key_rotation,
                 base::TimeDelta key_rotation_period) override;
  void Purge() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(KeyDataPrefsDelegateTest, Purge);

  void LoadKeysFromPrefs();

  // Updates the prefs stored for |project_name_hash|.
  void UpdatePrefsByProject(uint64_t project_name_hash,
                            const KeyProto& key_proto);

  raw_ptr<PrefService> local_state_;

  // Name of the preference to store the
  std::string pref_name_;

  // In-memory representation of the keys. Due to the StorageDelegate interface,
  // the prefs value is unable to be used directly.
  KeyDataProto proto_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PREFS_DELEGATE_H_
