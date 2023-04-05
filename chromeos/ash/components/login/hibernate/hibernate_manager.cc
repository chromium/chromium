// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/hibernate/hibernate_manager.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/hiberman/hiberman_client.h"

namespace ash {

HibernateManager::HibernateManager() = default;

HibernateManager::~HibernateManager() = default;

base::WeakPtr<HibernateManager> HibernateManager::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void HibernateManager::PrepareHibernateAndMaybeResumeAuthOp(
    std::unique_ptr<UserContext> user_context,
    AuthOperationCallback callback) {
  auto* client = HibermanClient::Get();
  if (!client || !client->IsAlive()) {
    std::move(callback).Run(std::move(user_context), absl::nullopt);
    return;
  }

  PrepareHibernateAndMaybeResume(
      std::move(user_context),
      base::BindOnce(&HibernateManager::ResumeFromHibernateAuthOpCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void HibernateManager::PrepareHibernateAndMaybeResume(
    std::unique_ptr<UserContext> user_context,
    HibernateResumeCallback callback) {
  auto* client = HibermanClient::Get();
  if (!client || !client->IsAlive()) {
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
