// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/hibernate/hibernate_manager.h"

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/hiberman/hiberman_client.h"

namespace ash {

namespace {
constexpr const char kFeatureNotEnabled[] = "hibernate feature not enabled";
constexpr const char kHibermanNotReady[] = "hiberman was not ready";
constexpr const char kSystemMissingDevSnapshot[] =
    "system is missing /dev/snapshot";

constexpr const char kDevSnapshotPath[] = "/dev/snapshot";
constexpr const char kHibermanBinaryPath[] = "/usr/sbin/hiberman";

constexpr const char kEnableSuspendToDiskInternalName[] =
    "enable-suspend-to-disk";

// Returns true if a /dev/snapshot node exists. We can't hibernate without one
// so no need to proceed if not.
bool HasSnapshotDevice() {
  static bool hasSnapshotDev = []() -> bool {
    return base::PathExists(base::FilePath(kDevSnapshotPath));
  }();
  return hasSnapshotDev;
}

// Returns true if the system has a hiberman binary.
bool HasHibermanBinary() {
  static bool hasHibermanBinary = []() -> bool {
    return base::PathExists(base::FilePath(kHibermanBinaryPath));
  }();
  return hasHibermanBinary;
}

bool g_platform_support_test_complete = false;

}  // namespace

HibernateManager::HibernateManager() = default;

HibernateManager::~HibernateManager() = default;

HibernateManager* HibernateManager::Get() {
  static base::NoDestructor<HibernateManager> hibernate;
  return hibernate.get();
}

void HibernateManager::InitializePlatformSupport() {
  HasSnapshotDevice();
  HasHibermanBinary();
  g_platform_support_test_complete = true;
}

// static
bool HibernateManager::IsHibernateSupported() {
  return ash::HasHibermanBinary();
}

void HibernateManager::SetAuthInfo(const std::string& account_id,
                                   const std::string& auth_session_id) {
  account_id_ = account_id;
  auth_session_id_ = auth_session_id;
}

void HibernateManager::MaybeResume(const std::set<std::string>& user_prefs) {
  if (auth_session_id_.empty()) {
    return;
  }

  auto* client = HibermanClient::Get();
  bool aborted = false;

  bool enabled = client->IsEnabled();

  for (const auto& flag : user_prefs) {
    if (base::StartsWith(
            flag, base::StrCat({kEnableSuspendToDiskInternalName, "@"}))) {
      enabled = !base::EndsWith(flag, "@0");
    }
  }

  if (!client) {
    aborted = true;
  } else if (!client->IsAlive() || !g_platform_support_test_complete) {
    aborted = true;
    client->AbortResumeHibernate(kHibermanNotReady);
  } else if (!enabled) {
    aborted = true;
    client->AbortResumeHibernate(kFeatureNotEnabled);
  } else if (!HasSnapshotDevice()) {
    aborted = true;
    client->AbortResumeHibernate(kSystemMissingDevSnapshot);
  }

  if (!aborted) {
    client->ResumeFromHibernate(account_id_, auth_session_id_);
  }

  account_id_.clear();
  auth_session_id_.clear();
}

}  // namespace ash
