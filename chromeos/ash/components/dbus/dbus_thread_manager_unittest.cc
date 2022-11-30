// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/dbus_thread_manager.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(DBusThreadManagerTest, Initialize) {
  DBusThreadManager::Initialize();
  EXPECT_TRUE(DBusThreadManager::IsInitialized());

  DBusThreadManager* manager = DBusThreadManager::Get();
  ASSERT_TRUE(manager);

  // In tests, clients are fake.
  EXPECT_TRUE(manager->IsUsingFakes());

  DBusThreadManager::Shutdown();
  EXPECT_FALSE(DBusThreadManager::IsInitialized());
}

}  // namespace ash
