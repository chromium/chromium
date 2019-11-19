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
  EXPECT_TRUE(manager->GetArcMidisClient());
  EXPECT_TRUE(manager->GetArcObbMounterClient());
  EXPECT_TRUE(manager->GetArcOemCryptoClient());
  EXPECT_TRUE(manager->GetCiceroneClient());
  EXPECT_TRUE(manager->GetConciergeClient());
  EXPECT_TRUE(manager->GetCrosDisksClient());
  EXPECT_TRUE(manager->GetDebugDaemonClient());
  EXPECT_TRUE(manager->GetEasyUnlockClient());
  EXPECT_TRUE(manager->GetImageBurnerClient());
  EXPECT_TRUE(manager->GetLorgnetteManagerClient());
  EXPECT_TRUE(manager->GetModemMessagingClient());
  EXPECT_TRUE(manager->GetSeneschalClient());
  EXPECT_TRUE(manager->GetShillDeviceClient());
  EXPECT_TRUE(manager->GetShillIPConfigClient());
  EXPECT_TRUE(manager->GetShillManagerClient());
  EXPECT_TRUE(manager->GetShillServiceClient());
  EXPECT_TRUE(manager->GetShillProfileClient());
  EXPECT_TRUE(manager->GetShillThirdPartyVpnDriverClient());
  EXPECT_TRUE(manager->GetSMSClient());
  EXPECT_TRUE(manager->GetUpdateEngineClient());

  DBusThreadManager::Shutdown();
  EXPECT_FALSE(DBusThreadManager::IsInitialized());
}

// Tests that clients can be created for the browser process.
TEST(DBusThreadManagerTest, InitializeForBrowser) {
  DBusThreadManager::Initialize(DBusThreadManager::kAll);
  DBusThreadManager* manager = DBusThreadManager::Get();
  ASSERT_TRUE(manager);

  // Common clients were created.
  EXPECT_TRUE(manager->GetModemMessagingClient());
  EXPECT_TRUE(manager->GetShillDeviceClient());
  EXPECT_TRUE(manager->GetShillIPConfigClient());
  EXPECT_TRUE(manager->GetShillManagerClient());
  EXPECT_TRUE(manager->GetShillProfileClient());
  EXPECT_TRUE(manager->GetShillServiceClient());
  EXPECT_TRUE(manager->GetShillThirdPartyVpnDriverClient());
  EXPECT_TRUE(manager->GetSMSClient());
  EXPECT_TRUE(manager->GetUpdateEngineClient());

  // Clients for the browser were created.
  EXPECT_TRUE(manager->GetArcMidisClient());
  EXPECT_TRUE(manager->GetArcObbMounterClient());
  EXPECT_TRUE(manager->GetArcOemCryptoClient());
  EXPECT_TRUE(manager->GetCiceroneClient());
  EXPECT_TRUE(manager->GetConciergeClient());
  EXPECT_TRUE(manager->GetCrosDisksClient());
  EXPECT_TRUE(manager->GetDebugDaemonClient());
  EXPECT_TRUE(manager->GetEasyUnlockClient());
  EXPECT_TRUE(manager->GetImageBurnerClient());
  EXPECT_TRUE(manager->GetLorgnetteManagerClient());
  EXPECT_TRUE(manager->GetSeneschalClient());

  DBusThreadManager::Shutdown();
}

// Tests that clients can be created for the ash process.
TEST(DBusThreadManagerTest, InitializeForAsh) {
  DBusThreadManager::Initialize(DBusThreadManager::kShared);
  DBusThreadManager* manager = DBusThreadManager::Get();
  ASSERT_TRUE(manager);

  // Common clients were created.
  EXPECT_TRUE(manager->GetModemMessagingClient());
  EXPECT_TRUE(manager->GetShillDeviceClient());
  EXPECT_TRUE(manager->GetShillIPConfigClient());
  EXPECT_TRUE(manager->GetShillManagerClient());
  EXPECT_TRUE(manager->GetShillProfileClient());
  EXPECT_TRUE(manager->GetShillServiceClient());
  EXPECT_TRUE(manager->GetShillThirdPartyVpnDriverClient());
  EXPECT_TRUE(manager->GetSMSClient());

  // Clients for other processes were not created.
  EXPECT_FALSE(manager->GetArcMidisClient());
  EXPECT_FALSE(manager->GetArcObbMounterClient());
  EXPECT_FALSE(manager->GetArcOemCryptoClient());
  EXPECT_FALSE(manager->GetCiceroneClient());
  EXPECT_FALSE(manager->GetConciergeClient());
  EXPECT_FALSE(manager->GetCrosDisksClient());
  EXPECT_FALSE(manager->GetDebugDaemonClient());
  EXPECT_FALSE(manager->GetEasyUnlockClient());
  EXPECT_FALSE(manager->GetImageBurnerClient());
  EXPECT_FALSE(manager->GetLorgnetteManagerClient());
  EXPECT_FALSE(manager->GetSeneschalClient());
  EXPECT_FALSE(manager->GetUpdateEngineClient());

  DBusThreadManager::Shutdown();
}

}  // namespace chromeos
