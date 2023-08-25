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
constexpr const char kSystemHasAESKL[] = "system is using aeskl";
constexpr const char kSystemMissingDevSnapshot[] =
    "system is missing /dev/snapshot";

constexpr const char kCryptoPath[] = "/proc/crypto";
constexpr const char kDevSnapshotPath[] = "/dev/snapshot";
constexpr const char kHibermanBinaryPath[] = "/usr/sbin/hiberman";

constexpr const char kEnableSuspendToDiskInternalName[] =
    "enable-suspend-to-disk";
constexpr const char kEnableSuspendToDiskAllowS4InternalName[] =
    "enable-suspend-to-disk-allow-s4";

// HasAESKL will return true if the system is using aeskl (AES w/
// KeyLocker). The reason for this is because keylocker requires suspend to
// S4 meaning that platform state is retained. We are currently only
// hibernating to S5 making it incompatible with keylocker.
bool HasAESKL() {
  static bool hasKL = []() -> bool {
    base::FilePath file_path = base::FilePath(kCryptoPath);

    std::string crypto_info;
    if (!base::ReadFileToStringNonBlocking(base::FilePath(file_path),
                                           &crypto_info)) {
      PLOG(ERROR) << "Failed to read from: " << file_path;
      return false;
    }

    return base::Contains(crypto_info, "aeskl");
  }();
  return hasKL;
}

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
  HasAESKL();
  HasHibermanBinary();
  g_platform_support_test_complete = true;
}

// static
bool HibernateManager::HasAESKL() {
  return ash::HasAESKL();
}

// static
bool HibernateManager::IsHibernateSupported() {
  return ash::HasHibermanBinary();
}

void HibernateManager::SetAuthSessionID(const std::string& auth_session_id) {
  auth_session_id_ = auth_session_id;
}

void HibernateManager::MaybeResume(const std::set<std::string>& user_prefs) {
  if (auth_session_id_.empty()) {
    return;
  }

  auto* client = HibermanClient::Get();
  bool aborted = false;

  bool enabled = client->IsEnabled();
  bool s4_enabled = client->IsHibernateToS4Enabled();

  for (const auto& flag : user_prefs) {
    if (base::StartsWith(
            flag, base::StrCat({kEnableSuspendToDiskInternalName, "@"}))) {
      enabled = !base::EndsWith(flag, "@0");
    } else if (base::StartsWith(
                   flag, base::StrCat(
                             {kEnableSuspendToDiskAllowS4InternalName, "@"}))) {
      s4_enabled = !base::EndsWith(flag, "@0");
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
  } else if (HasAESKL() && !s4_enabled) {
    aborted = true;
    client->AbortResumeHibernate(kSystemHasAESKL);
  } else if (!HasSnapshotDevice()) {
    aborted = true;
    client->AbortResumeHibernate(kSystemMissingDevSnapshot);
  }

  if (!aborted) {
    client->ResumeFromHibernate(auth_session_id_);
  }

  auth_session_id_.clear();
}

}  // namespace ash
