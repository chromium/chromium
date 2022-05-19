// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

// Tests that real and fake clients can be created.
TEST(DBusThreadManagerTest, Initialize) {
  DBusThreadManager::Initialize();
  EXPECT_TRUE(DBusThreadManager::IsInitialized());

  DBusThreadManager* manager = DBusThreadManager::Get();
  ASSERT_TRUE(manager);

  // In tests, clients are fake.
  EXPECT_TRUE(manager->IsUsingFakes());

  // Clients were created.
  EXPECT_TRUE(manager->GetAnomalyDetectorClient());
  EXPECT_TRUE(manager->GetArcMidisClient());
  EXPECT_TRUE(manager->GetArcObbMounterClient());
  EXPECT_TRUE(manager->GetCrosDisksClient());
  EXPECT_TRUE(manager->GetDebugDaemonClient());
  EXPECT_TRUE(manager->GetEasyUnlockClient());
  EXPECT_TRUE(manager->GetImageBurnerClient());
  EXPECT_TRUE(manager->GetLorgnetteManagerClient());
  EXPECT_TRUE(manager->GetUpdateEngineClient());

  DBusThreadManager::Shutdown();
  EXPECT_FALSE(DBusThreadManager::IsInitialized());
}

}  // namespace chromeos
