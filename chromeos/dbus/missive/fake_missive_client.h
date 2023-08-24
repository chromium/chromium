// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MISSIVE_FAKE_MISSIVE_CLIENT_H_
#define CHROMEOS_DBUS_MISSIVE_FAKE_MISSIVE_CLIENT_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace chromeos {

// Fake implementation of MissiveClient. This is currently a no-op fake.
class COMPONENT_EXPORT(MISSIVE) FakeMissiveClient
    : public MissiveClient,
      public MissiveClient::TestInterface {
 public:
  FakeMissiveClient();
  ~FakeMissiveClient() override;

  FakeMissiveClient(const FakeMissiveClient& other) = delete;
  FakeMissiveClient& operator=(const FakeMissiveClient& other) = delete;

  void Init();

  // MissiveClient implementation:
  void EnqueueRecord(
      const reporting::Priority priority,
      reporting::Record record,
      base::OnceCallback<void(reporting::Status)> completion_callback) override;
  void Flush(
      const reporting::Priority priority,
      base::OnceCallback<void(reporting::Status)> completion_callback) override;
  void UpdateEncryptionKey(
      const reporting::SignedEncryptionInfo& encryption_info) override;
  void UpdateConfigInMissive(
      const reporting::ListOfBlockedDestinations& destinations) override;
  void ReportSuccess(const reporting::SequenceInformation& sequence_information,
                     bool force_confirm) override;
  TestInterface* GetTestInterface() override;
  base::WeakPtr<MissiveClient> GetWeakPtr() override;

  // |MissiveClient::TestInterface| implementation:
  const std::vector<::reporting::Record>& GetEnqueuedRecords(
      ::reporting::Priority priority) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  base::flat_map<::reporting::Priority, std::vector<::reporting::Record>>
      enqueued_records_;
  base::ObserverList<Observer> observer_list_;

  // Weak pointer factory - must be last member of the class.
  base::WeakPtrFactory<FakeMissiveClient> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MISSIVE_FAKE_MISSIVE_CLIENT_H_
