// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_SYSTEM_TOKEN_CERT_DB_STORAGE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_SYSTEM_TOKEN_CERT_DB_STORAGE_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace net {
class NSSCertDatabase;
}

namespace ash {

// Used by SystemTokenCertDbInitializer to save the system token certificate
// database when it is ready.
// This class is following the singleton pattern. The single global instance is
// initialized and destroyed by `ChromeBrowserMainPartsAsh`.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) SystemTokenCertDbStorage {
 public:
  // An observer that gets notified when the global NSSCertDatabase is about to
  // be destroyed.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the global NSSCertDatabase is about to be destroyed.
    // Consumers of that database should drop any reference to it and stop using
    // it.
    virtual void OnSystemTokenCertDbDestroyed() = 0;
  };

  SystemTokenCertDbStorage(const SystemTokenCertDbStorage&) = delete;
  SystemTokenCertDbStorage& operator=(const SystemTokenCertDbStorage&) = delete;

  // It is stated in cryptohome implementation that 5 minutes is enough time to
  // wait for any TPM operations. For more information, please refer to:
  // https://chromium.googlesource.com/chromiumos/platform2/+/main/cryptohome/cryptohome.cc
  static constexpr base::TimeDelta kMaxCertDbRetrievalDelay = base::Minutes(5);

  // Called by `ChromeBrowserMainPartsAsh` to initialize a global
  // SystemTokenCertDbStorage instance.
  static void Initialize();

  // Called by `ChromeBrowserMainPartsAsh` to delete the global
  // SystemTokenCertDbStorage instance.
  static void Shutdown();

  // Returns a global instance. May return null if not initialized.
  static SystemTokenCertDbStorage* Get();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Used by SystemTokenCertDbInitializer to save the system token certificate
  // database when it is ready.
  // Note: This method is expected to be called only once by the
  // SystemTokenCertDbInitializer.
  void SetDatabase(net::NSSCertDatabase* system_token_cert_database);

  // Used by SystemTokenCertDbInitializer to reset the system token certificate
  // database and notify observers that it is not usable anymore.
  void ResetDatabase();

  // Retrieves the global NSSCertDatabase for the system token and passes it to
  // |callback|. If the database is already initialized, calls |callback|
  // immediately. Otherwise, |callback| will be called with the database when it
  // is initialized or with a nullptr if the initialization failed.
  // To be notified when the returned NSSCertDatabase becomes invalid, callers
  // should register as Observer.
  using GetDatabaseCallback =
      base::OnceCallback<void(net::NSSCertDatabase* nss_cert_database)>;
  void GetDatabase(GetDatabaseCallback callback);

 private:
  SystemTokenCertDbStorage();
  ~SystemTokenCertDbStorage();

  // Called after a delay if the system token certificate database was still not
  // initialized when |GetDatabase| was called. This function notifies
  // |get_system_token_cert_db_callback_list_| with nullptrs as a way of
  // informing callers that the database initialization failed.
  void OnSystemTokenDbRetrievalTimeout();

  // List of callbacks that should be executed when the system token certificate
  // database is created.
  base::OnceCallbackList<GetDatabaseCallback::RunType>
      get_system_token_cert_db_callback_list_;

  // List of observers that will be notified when the global system token
  // NSSCertDatabase is destroyed.
  base::ObserverList<Observer> observers_;

  // Global NSSCertDatabase which sees the system token. Owned by
  // SystemTokenCertDbInitializer.
  raw_ptr<net::NSSCertDatabase> system_token_cert_database_ = nullptr;

  bool system_token_cert_db_retrieval_failed_ = false;

  base::OneShotTimer system_token_cert_db_retrieval_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_SYSTEM_TOKEN_CERT_DB_STORAGE_H_
