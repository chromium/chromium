// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/user_prefs.h"

#include "base/functional/bind.h"
#include "base/supports_user_data.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_prefs {

namespace {

class UserPrefsTest : public testing::Test {
 public:
  UserPrefsTest() = default;
  ~UserPrefsTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;

  class TestContext : public base::SupportsUserData {
   public:
    TestContext() = default;
    ~TestContext() override = default;
  };

  std::unique_ptr<PrefService> CreatePrefService(
      scoped_refptr<TestingPrefStore> user_pref_store,
      bool async) {
    auto pref_notifier = std::make_unique<PrefNotifierImpl>();
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    auto managed_prefs = base::MakeRefCounted<TestingPrefStore>();
    auto supervised_user_prefs = base::MakeRefCounted<TestingPrefStore>();
    auto extension_prefs = base::MakeRefCounted<TestingPrefStore>();
    auto recommended_prefs = base::MakeRefCounted<TestingPrefStore>();
    auto pref_value_store = std::make_unique<PrefValueStore>(
        managed_prefs.get(), supervised_user_prefs.get(), extension_prefs.get(),
        /*command_line_prefs=*/nullptr, user_pref_store.get(),
        recommended_prefs.get(), pref_registry->defaults(),
        pref_notifier.get());

    return std::make_unique<PrefService>(
        std::move(pref_notifier), std::move(pref_value_store), user_pref_store,
        pref_registry,
        base::BindRepeating([](PersistentPrefStore::PrefReadError) {}), async);
  }
};

TEST_F(UserPrefsTest, ArePrefsLoaded) {
  // 1. Initially not initialized.
  {
    TestContext context;
    EXPECT_FALSE(UserPrefs::IsInitialized(&context));
    EXPECT_FALSE(UserPrefs::ArePrefsLoaded(&context));
  }

  // 2. Default status is WAITING.
  {
    auto store = base::MakeRefCounted<TestingPrefStore>();
    store->SetBlockAsyncRead(true);
    std::unique_ptr<PrefService> prefs =
        CreatePrefService(store, /*async=*/true);
    TestContext context;
    UserPrefs::Set(&context, prefs.get());
    EXPECT_TRUE(UserPrefs::IsInitialized(&context));
    EXPECT_FALSE(UserPrefs::ArePrefsLoaded(&context));
  }
}

TEST_F(UserPrefsTest, ArePrefsLoaded_Statuses) {
  // status 1 = SUCCESS
  {
    auto store = base::MakeRefCounted<TestingPrefStore>();
    store->set_read_error(PersistentPrefStore::PREF_READ_ERROR_NONE);
    store->SetInitializationCompleted();
    std::unique_ptr<PrefService> prefs =
        CreatePrefService(store, /*async=*/false);
    TestContext context;
    UserPrefs::Set(&context, prefs.get());
    EXPECT_TRUE(UserPrefs::ArePrefsLoaded(&context));
  }

  // status 2 = CREATED_NEW_PREF_STORE
  {
    auto store = base::MakeRefCounted<TestingPrefStore>();
    store->set_read_error(PersistentPrefStore::PREF_READ_ERROR_NO_FILE);
    store->SetInitializationCompleted();
    std::unique_ptr<PrefService> prefs =
        CreatePrefService(store, /*async=*/false);
    TestContext context;
    UserPrefs::Set(&context, prefs.get());
    EXPECT_TRUE(UserPrefs::ArePrefsLoaded(&context));
  }

  // status 3 = ERROR
  {
    auto store = base::MakeRefCounted<TestingPrefStore>();
    store->set_read_error(PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE);
    store->SetInitializationCompleted();
    std::unique_ptr<PrefService> prefs =
        CreatePrefService(store, /*async=*/false);
    TestContext context;
    UserPrefs::Set(&context, prefs.get());
    EXPECT_FALSE(UserPrefs::ArePrefsLoaded(&context));
  }
}

}  // namespace
}  // namespace user_prefs
