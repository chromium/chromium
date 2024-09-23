// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_TESTING_PREF_STORE_H_
#define COMPONENTS_PREFS_TESTING_PREF_STORE_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_value_map.h"

// |TestingPrefStore| is a preference store implementation that allows tests to
// explicitly manipulate the contents of the store, triggering notifications
// where appropriate.
class TestingPrefStore : public PersistentPrefStore {
 public:
  TestingPrefStore();

  TestingPrefStore(const TestingPrefStore&) = delete;
  TestingPrefStore& operator=(const TestingPrefStore&) = delete;

  // Overridden from PrefStore.
  bool GetValue(std::string_view key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

  // PersistentPrefStore overrides:
  bool GetMutableValue(std::string_view key, base::Value** result) override;
  void ReportValueChanged(std::string_view key, uint32_t flags) override;
  void SetValue(std::string_view key,
                base::Value value,
                uint32_t flags) override;
  void SetValueSilently(std::string_view key,
                        base::Value value,
                        uint32_t flags) override;
  void RemoveValue(std::string_view key, uint32_t flags) override;
  void RemoveValuesByPrefixSilently(std::string_view prefix) override;
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  PersistentPrefStore::PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  void CommitPendingWrite(base::OnceClosure reply_callback,
                          base::OnceClosure synchronous_done_callback) override;
  void SchedulePendingLossyWrites() override;
  bool HasReadErrorDelegate() const override;

  // Marks the store as having completed initialization.
  void SetInitializationCompleted();

  // Used for tests to trigger notifications explicitly.
  void NotifyPrefValueChanged(std::string_view key);
  void NotifyInitializationCompleted();

  // Some convenience getters/setters.
  void SetString(const std::string& key, const std::string& value);
  void SetInteger(const std::string& key, int value);
  void SetBoolean(const std::string& key, bool value);

  bool GetString(const std::string& key, std::string* value) const;
  bool GetInteger(const std::string& key, int* value) const;
  bool GetBoolean(const std::string& key, bool* value) const;

  // Determines whether ReadPrefsAsync completes immediately. Defaults to false
  // (non-blocking). To block, invoke this with true (blocking) before the call
  // to ReadPrefsAsync. To unblock, invoke again with false (non-blocking) after
  // the call to ReadPrefsAsync.
  void SetBlockAsyncRead(bool block_async_read);

  // Waits until the pref at `key` changes its value.
  void WaitUntilValueChanges(std::string key);

  // Waits until the pref at `key` equals to the `expected_value`.
  //
  // Will exit immediately if the current value is already equal to the
  // `expected_value`.  Otherwise, spins up a RunLoop until the value changes to
  // the `expected_value`.
  void WaitForValue(std::string key, base::Value expected_value);

  void OnStoreDeletionFromDisk() override;

  // Getter and Setter methods for setting and getting the state of the
  // |TestingPrefStore|.
  virtual void set_read_only(bool read_only);
  void set_read_success(bool read_success);
  void set_read_error(PersistentPrefStore::PrefReadError read_error);
  bool committed() { return committed_; }

 protected:
  ~TestingPrefStore() override;

 private:
  void CheckPrefIsSerializable(std::string_view key, const base::Value& value);

  // Stores the preference values.
  PrefValueMap prefs_;

  // Flag that indicates if the PrefStore is read-only
  bool read_only_;

  // The result to pass to PrefStore::Observer::OnInitializationCompleted
  bool read_success_;

  // The result to return from ReadPrefs or ReadPrefsAsync.
  PersistentPrefStore::PrefReadError read_error_;

  // Whether a call to ReadPrefsAsync should block.
  bool block_async_read_;

  // Whether there is a pending call to ReadPrefsAsync.
  bool pending_async_read_;

  // Whether initialization has been completed.
  bool init_complete_;

  // Whether the store contents have been committed to disk since the last
  // mutation.
  bool committed_;

  // Optional so we can differentiate `nullopt` from `nullptr`.
  std::optional<std::unique_ptr<ReadErrorDelegate>> error_delegate_;
  base::ObserverList<PrefStore::Observer, true> observers_;
};

#endif  // COMPONENTS_PREFS_TESTING_PREF_STORE_H_
