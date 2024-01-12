// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_SYSTEM_TOKEN_CERT_DB_STORAGE_TEST_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_SYSTEM_TOKEN_CERT_DB_STORAGE_TEST_UTIL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"

#include "memory"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"

namespace net {
class NSSCertDatabase;
}

namespace ash {

// A helper that wraps the callback passed to
// SystemTokenCertDbStorage::GetDatabase and can answer queries
// regarding the state of the callback and database passed to the callback.
class GetSystemTokenCertDbCallbackWrapper {
 public:
  GetSystemTokenCertDbCallbackWrapper();
  GetSystemTokenCertDbCallbackWrapper(
      const GetSystemTokenCertDbCallbackWrapper& other) = delete;
  GetSystemTokenCertDbCallbackWrapper& operator=(
      const GetSystemTokenCertDbCallbackWrapper& other) = delete;
  ~GetSystemTokenCertDbCallbackWrapper();

  SystemTokenCertDbStorage::GetDatabaseCallback GetCallback();

  // Waits until the callback returned by GetCallback() has been called.
  void Wait();

  bool IsCallbackCalled() const;
  bool IsDbRetrievalSucceeded() const;

 private:
  void OnDbRetrieved(net::NSSCertDatabase* nss_cert_database);

  base::RunLoop run_loop_;
  bool done_ = false;
  raw_ptr<net::NSSCertDatabase> nss_cert_database_ = nullptr;

  base::WeakPtrFactory<GetSystemTokenCertDbCallbackWrapper> weak_ptr_factory_{
      this};
};

class FakeSystemTokenCertDbStorageObserver
    : public SystemTokenCertDbStorage::Observer {
 public:
  FakeSystemTokenCertDbStorageObserver();
  FakeSystemTokenCertDbStorageObserver(
      const FakeSystemTokenCertDbStorageObserver& other) = delete;
  FakeSystemTokenCertDbStorageObserver& operator=(
      const FakeSystemTokenCertDbStorageObserver& other) = delete;
  ~FakeSystemTokenCertDbStorageObserver() override;

  // Waits until the observer has been notified with .
  bool HasBeenNotified();

 private:
  // SystemTokenCertDbStorage::Obsever:
  void OnSystemTokenCertDbDestroyed() override;

  bool has_been_notified_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_SYSTEM_TOKEN_CERT_DB_STORAGE_TEST_UTIL_H_
