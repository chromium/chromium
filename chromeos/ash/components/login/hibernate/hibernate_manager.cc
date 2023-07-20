// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/hibernate/hibernate_manager.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
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

    return (crypto_info.find("aeskl") != std::string::npos);
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

HibernateManager::HibernateManager() {}

HibernateManager::~HibernateManager() = default;

base::WeakPtr<HibernateManager> HibernateManager::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

void HibernateManager::PrepareHibernateAndMaybeResumeAuthOp(
    std::unique_ptr<UserContext> user_context,
    AuthOperationCallback callback) {
  PrepareHibernateAndMaybeResume(
      std::move(user_context),
      base::BindOnce(&HibernateManager::ResumeFromHibernateAuthOpCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void HibernateManager::PrepareHibernateAndMaybeResume(
    std::unique_ptr<UserContext> user_context,
    HibernateResumeCallback callback) {
  auto* client = HibermanClient::Get();
  bool aborted = false;

  if (!client) {
    aborted = true;
  } else if (!client->IsAlive() || !g_platform_support_test_complete) {
    aborted = true;
    client->AbortResumeHibernate(kHibermanNotReady);
  } else if (HasAESKL() && !client->IsHibernateToS4Enabled()) {
    aborted = true;
    client->AbortResumeHibernate(kSystemHasAESKL);
  } else if (!HasSnapshotDevice()) {
    aborted = true;
    client->AbortResumeHibernate(kSystemMissingDevSnapshot);
  } else if (!client->IsEnabled()) {
    aborted = true;
    client->AbortResumeHibernate(kFeatureNotEnabled);
  }

  if (aborted) {
    // Always run the callback so we don't block login.
    std::move(callback).Run(std::move(user_context), true);
    return;
  }

  // In a successful resume case, this function never returns, as execution
  // continues in the resumed hibernation image.
  client->ResumeFromHibernateAS(
      user_context->GetAuthSessionId(),
      base::BindOnce(std::move(callback), std::move(user_context)));
}

void HibernateManager::ResumeFromHibernateAuthOpCallback(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> user_context,
    bool resume_call_successful) {
  std::move(callback).Run(std::move(user_context), absl::nullopt);
}

}  // namespace ash
