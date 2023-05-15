// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/system_token_cert_db_storage.h"

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "net/cert/nss_cert_database.h"

namespace ash {

namespace {

// Owned `ChromeBrowserMainPartsAsh`.
SystemTokenCertDbStorage* g_system_token_cert_db_storage = nullptr;

}  // namespace

// static
constexpr base::TimeDelta SystemTokenCertDbStorage::kMaxCertDbRetrievalDelay;

// static
void SystemTokenCertDbStorage::Initialize() {
  DCHECK_EQ(g_system_token_cert_db_storage, nullptr);
  g_system_token_cert_db_storage = new SystemTokenCertDbStorage();
}

// static
void SystemTokenCertDbStorage::Shutdown() {
  DCHECK_NE(g_system_token_cert_db_storage, nullptr);
  delete g_system_token_cert_db_storage;
  g_system_token_cert_db_storage = nullptr;
}

// static
SystemTokenCertDbStorage* SystemTokenCertDbStorage::Get() {
  return g_system_token_cert_db_storage;
}

void SystemTokenCertDbStorage::SetDatabase(
    net::NSSCertDatabase* system_token_cert_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(system_token_cert_database_, nullptr);
  system_token_cert_database_ = std::move(system_token_cert_database);
  system_token_cert_db_retrieval_timer_.Stop();
  get_system_token_cert_db_callback_list_.Notify(system_token_cert_database_);
}

void SystemTokenCertDbStorage::ResetDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  system_token_cert_database_ = nullptr;
  // If any consumer asks for the database between now and when
  // SystemTokenCertDbStorage is destroyed, respond with a failure.
  system_token_cert_db_retrieval_failed_ = true;

  // Notify observers that the SystemTokenCertDbStorage and the
  // NSSCertDatabase it provides can not be used anymore.
  for (auto& observer : g_system_token_cert_db_storage->observers_)
    observer.OnSystemTokenCertDbDestroyed();
}

void SystemTokenCertDbStorage::GetDatabase(GetDatabaseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(callback);

  if (system_token_cert_database_) {
    std::move(callback).Run(system_token_cert_database_.get());
  } else if (system_token_cert_db_retrieval_failed_) {
    std::move(callback).Run(/*nss_cert_database=*/nullptr);
  } else {
    get_system_token_cert_db_callback_list_.AddUnsafe(std::move(callback));

    if (!system_token_cert_db_retrieval_timer_.IsRunning()) {
      system_token_cert_db_retrieval_timer_.Start(
          FROM_HERE, kMaxCertDbRetrievalDelay, /*receiver=*/this,
          &SystemTokenCertDbStorage::OnSystemTokenDbRetrievalTimeout);
    }
  }
}

void SystemTokenCertDbStorage::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
}

void SystemTokenCertDbStorage::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}

SystemTokenCertDbStorage::SystemTokenCertDbStorage() = default;

SystemTokenCertDbStorage::~SystemTokenCertDbStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SystemTokenCertDbStorage::OnSystemTokenDbRetrievalTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  system_token_cert_db_retrieval_failed_ = true;
  get_system_token_cert_db_callback_list_.Notify(
      /*nss_cert_database=*/nullptr);
}

}  // namespace ash
