// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/default_pref_store.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;

namespace {

class MockPrefStoreObserver : public PrefStore::Observer {
 public:
  explicit MockPrefStoreObserver(DefaultPrefStore* pref_store);

  MockPrefStoreObserver(const MockPrefStoreObserver&) = delete;
  MockPrefStoreObserver& operator=(const MockPrefStoreObserver&) = delete;

  ~MockPrefStoreObserver() override;

  int change_count() {
    return change_count_;
  }

  // PrefStore::Observer implementation:
  void OnPrefValueChanged(std::string_view key) override;

 private:
  raw_ptr<DefaultPrefStore> pref_store_;

  int change_count_;
};

MockPrefStoreObserver::MockPrefStoreObserver(DefaultPrefStore* pref_store)
    : pref_store_(pref_store), change_count_(0) {
  pref_store_->AddObserver(this);
}

MockPrefStoreObserver::~MockPrefStoreObserver() {
  pref_store_->RemoveObserver(this);
}

void MockPrefStoreObserver::OnPrefValueChanged(std::string_view key) {
  change_count_++;
}

}  // namespace

TEST(DefaultPrefStoreTest, NotifyPrefValueChanged) {
  scoped_refptr<DefaultPrefStore> pref_store(new DefaultPrefStore);
  MockPrefStoreObserver observer(pref_store.get());
  std::string kPrefKey("pref_key");

  // Setting a default value shouldn't send a change notification.
  pref_store->SetDefaultValue(kPrefKey, Value("foo"));
  EXPECT_EQ(0, observer.change_count());

  // Replacing the default value should send a change notification...
  pref_store->ReplaceDefaultValue(kPrefKey, Value("bar"));
  EXPECT_EQ(1, observer.change_count());

  // But only if the value actually changed.
  pref_store->ReplaceDefaultValue(kPrefKey, Value("bar"));
  EXPECT_EQ(1, observer.change_count());
}
