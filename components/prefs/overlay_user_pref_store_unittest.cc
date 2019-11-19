// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/overlay_user_pref_store.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store_unittest.h"
#include "components/prefs/pref_store_observer_mock.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::StrEq;

namespace base {
namespace {

const char kBrowserWindowPlacement[] = "browser.window_placement";
const char kShowBookmarkBar[] = "bookmark_bar.show_on_all_tabs";
const char kSharedKey[] = "sync_promo.show_on_first_run_allowed";

const char* const regular_key = kBrowserWindowPlacement;
const char* const persistent_key = kShowBookmarkBar;
const char* const shared_key = kSharedKey;

}  // namespace

class OverlayUserPrefStoreTest : public testing::Test {
 protected:
  OverlayUserPrefStoreTest()
      : underlay_(new TestingPrefStore()),
        overlay_(new OverlayUserPrefStore(underlay_.get())) {
    overlay_->RegisterPersistentPref(persistent_key);
    overlay_->RegisterPersistentPref(shared_key);
  }

  ~OverlayUserPrefStoreTest() override {}

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestingPrefStore> underlay_;
  scoped_refptr<OverlayUserPrefStore> overlay_;
};

TEST_F(OverlayUserPrefStoreTest, Observer) {
  PrefStoreObserverMock obs;
  overlay_->AddObserver(&obs);

  // Check that underlay first value is reported.
  underlay_->SetValue(regular_key, std::make_unique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(regular_key);

  // Check that underlay overwriting is reported.
  underlay_->SetValue(regular_key, std::make_unique<Value>(43),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(regular_key);

  // Check that overwriting change in overlay is reported.
  overlay_->SetValue(regular_key, std::make_unique<Value>(44),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(regular_key);

  // Check that hidden underlay change is not reported.
  underlay_->SetValue(regular_key, std::make_unique<Value>(45),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());

  // Check that overlay remove is reported.
  overlay_->RemoveValue(regular_key,
                        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(regular_key);

  // Check that underlay remove is reported.
  underlay_->RemoveValue(regular_key,
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(regular_key);

  // Check respecting of silence.
  overlay_->SetValueSilently(regular_key, std::make_unique<Value>(46),
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());

  overlay_->RemoveObserver(&obs);

  // Check successful unsubscription.
  underlay_->SetValue(regular_key, std::make_unique<Value>(47),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(regular_key, std::make_unique<Value>(48),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());
}

TEST_F(OverlayUserPrefStoreTest, GetAndSet) {
  const Value* value = nullptr;
  EXPECT_FALSE(overlay_->GetValue(regular_key, &value));
  EXPECT_FALSE(underlay_->GetValue(regular_key, &value));

  underlay_->SetValue(regular_key, std::make_unique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  // Value shines through:
  EXPECT_TRUE(overlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  EXPECT_TRUE(underlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  overlay_->SetValue(regular_key, std::make_unique<Value>(43),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  EXPECT_TRUE(overlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));

  EXPECT_TRUE(underlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  overlay_->RemoveValue(regular_key,
                        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  // Value shines through:
  EXPECT_TRUE(overlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  EXPECT_TRUE(underlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));
}

// Check that GetMutableValue does not return the dictionary of the underlay.
TEST_F(OverlayUserPrefStoreTest, ModifyDictionaries) {
  underlay_->SetValue(regular_key, std::make_unique<DictionaryValue>(),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  Value* modify = nullptr;
  EXPECT_TRUE(overlay_->GetMutableValue(regular_key, &modify));
  ASSERT_TRUE(modify);
  ASSERT_TRUE(modify->is_dict());
  static_cast<DictionaryValue*>(modify)->SetInteger(regular_key, 42);

  Value* original_in_underlay = nullptr;
  EXPECT_TRUE(underlay_->GetMutableValue(regular_key, &original_in_underlay));
  ASSERT_TRUE(original_in_underlay);
  ASSERT_TRUE(original_in_underlay->is_dict());
  EXPECT_TRUE(static_cast<DictionaryValue*>(original_in_underlay)->empty());

  Value* modified = nullptr;
  EXPECT_TRUE(overlay_->GetMutableValue(regular_key, &modified));
  ASSERT_TRUE(modified);
  ASSERT_TRUE(modified->is_dict());
  EXPECT_EQ(*modify, *modified);
}

// Here we consider a global preference that is not overlayed.
TEST_F(OverlayUserPrefStoreTest, GlobalPref) {
  PrefStoreObserverMock obs;
  overlay_->AddObserver(&obs);

  const Value* value = nullptr;

  // Check that underlay first value is reported.
  underlay_->SetValue(persistent_key, std::make_unique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(persistent_key);

  // Check that underlay overwriting is reported.
  underlay_->SetValue(persistent_key, std::make_unique<Value>(43),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(persistent_key);

  // Check that we get this value from the overlay
  EXPECT_TRUE(overlay_->GetValue(persistent_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));

  // Check that overwriting change in overlay is reported.
  overlay_->SetValue(persistent_key, std::make_unique<Value>(44),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(persistent_key);

  // Check that we get this value from the overlay and the underlay.
  EXPECT_TRUE(overlay_->GetValue(persistent_key, &value));
  EXPECT_TRUE(base::Value(44).Equals(value));
  EXPECT_TRUE(underlay_->GetValue(persistent_key, &value));
  EXPECT_TRUE(base::Value(44).Equals(value));

  // Check that overlay remove is reported.
  overlay_->RemoveValue(persistent_key,
                        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  obs.VerifyAndResetChangedKey(persistent_key);

  // Check that value was removed from overlay and underlay
  EXPECT_FALSE(overlay_->GetValue(persistent_key, &value));
  EXPECT_FALSE(underlay_->GetValue(persistent_key, &value));

  // Check respecting of silence.
  overlay_->SetValueSilently(persistent_key, std::make_unique<Value>(46),
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());

  overlay_->RemoveObserver(&obs);

  // Check successful unsubscription.
  underlay_->SetValue(persistent_key, std::make_unique<Value>(47),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(persistent_key, std::make_unique<Value>(48),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(obs.changed_keys.empty());
}

// Check that mutable values are removed correctly.
TEST_F(OverlayUserPrefStoreTest, ClearMutableValues) {
  // Set in overlay and underlay the same preference.
  underlay_->SetValue(regular_key, std::make_unique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(regular_key, std::make_unique<Value>(43),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  const Value* value = nullptr;
  // Check that an overlay preference is returned.
  EXPECT_TRUE(overlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));
  overlay_->ClearMutableValues();

  // Check that an underlay preference is returned.
  EXPECT_TRUE(overlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));
}

// Check that mutable values are removed correctly when using a silent set.
TEST_F(OverlayUserPrefStoreTest, ClearMutableValues_Silently) {
  // Set in overlay and underlay the same preference.
  underlay_->SetValueSilently(regular_key, std::make_unique<Value>(42),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValueSilently(regular_key, std::make_unique<Value>(43),
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  const Value* value = nullptr;
  // Check that an overlay preference is returned.
  EXPECT_TRUE(overlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));
  overlay_->ClearMutableValues();

  // Check that an underlay preference is returned.
  EXPECT_TRUE(overlay_->GetValue(regular_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));
}

TEST_F(OverlayUserPrefStoreTest, GetValues) {
  // To check merge behavior, create underlay and overlay so each has a key the
  // other doesn't have and they have one key in common.
  underlay_->SetValue(persistent_key, std::make_unique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(regular_key, std::make_unique<Value>(43),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  underlay_->SetValue(shared_key, std::make_unique<Value>(42),
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  overlay_->SetValue(shared_key, std::make_unique<Value>(43),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  auto values = overlay_->GetValues();
  const Value* value = nullptr;
  // Check that an overlay preference is returned.
  ASSERT_TRUE(values->Get(persistent_key, &value));
  EXPECT_TRUE(base::Value(42).Equals(value));

  // Check that an underlay preference is returned.
  ASSERT_TRUE(values->Get(regular_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));

  // Check that the overlay is preferred.
  ASSERT_TRUE(values->Get(shared_key, &value));
  EXPECT_TRUE(base::Value(43).Equals(value));
}

TEST_F(OverlayUserPrefStoreTest, CommitPendingWriteWithCallback) {
  TestCommitPendingWriteWithCallback(overlay_.get(), &task_environment_);
}

}  // namespace base
