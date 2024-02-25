// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_TEST_STORE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_TEST_STORE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/download/internal/background_service/store.h"

namespace download {

struct Entry;

namespace test {

class TestStore : public Store {
 public:
  TestStore();

  TestStore(const TestStore&) = delete;
  TestStore& operator=(const TestStore&) = delete;

  ~TestStore() override;

  // Store implementation.
  bool IsInitialized() override;
  void Initialize(InitCallback callback) override;
  void HardRecover(StoreCallback callback) override;
  void Update(const Entry& entry, StoreCallback callback) override;
  void Remove(const std::string& guid, StoreCallback callback) override;

  // Callback trigger methods.
  void AutomaticallyTriggerAllFutureCallbacks(bool success);
  void TriggerInit(bool success, std::unique_ptr<std::vector<Entry>> entries);
  void TriggerHardRecover(bool success);
  void TriggerUpdate(bool success);
  void TriggerRemove(bool success);

  // Parameter access methods.
  const Entry* LastUpdatedEntry() const;
  std::string LastRemovedEntry() const;
  bool init_called() const { return init_called_; }
  const std::vector<Entry>& updated_entries() const { return updated_entries_; }
  const std::vector<std::string>& removed_entries() const {
    return removed_entries_;
  }

 private:
  bool ready_;

  bool init_called_;

  std::vector<Entry> updated_entries_;
  std::vector<std::string> removed_entries_;

  std::optional<bool> automatic_callback_response_;
  InitCallback init_callback_;
  StoreCallback hard_recover_callback_;
  StoreCallback update_callback_;
  StoreCallback remove_callback_;
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_TEST_STORE_H_
