// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MISSIVE_MISSIVE_CLIENT_H_
#define CHROMEOS_DBUS_MISSIVE_MISSIVE_CLIENT_H_

#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/proto/synced/interface.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"

namespace dbus {
class Bus;
}

namespace chromeos {

BASE_DECLARE_FEATURE(kEnableRetryEnqueueRecord);

// D-Bus client for Missive service.
// Missive service provides a method for enterprise customers to locally encrypt
// and store |reporting::Record|s.
class COMPONENT_EXPORT(MISSIVE) MissiveClient {
 public:
  // Interface with testing functionality. Accessed through GetTestInterface(),
  // only implemented in the fake implementation.
  class TestInterface {
   public:
    class Observer : public base::CheckedObserver {
     public:
      virtual void OnRecordEnqueued(reporting::Priority priority,
                                    const reporting::Record& record) = 0;
    };

    virtual const std::vector<::reporting::Record>& GetEnqueuedRecords(
        ::reporting::Priority) = 0;

    virtual void AddObserver(Observer* observer) = 0;
    virtual void RemoveObserver(Observer* observer) = 0;

   protected:
    virtual ~TestInterface() = default;
  };

  MissiveClient(const MissiveClient& other) = delete;
  MissiveClient& operator=(const MissiveClient& other) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static MissiveClient* Get();

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

  virtual void EnqueueRecord(
      const reporting::Priority priority,
      reporting::Record record,
      base::OnceCallback<void(reporting::Status)> completion_callback) = 0;
  virtual void Flush(
      const reporting::Priority priority,
      base::OnceCallback<void(reporting::Status)> completion_callback) = 0;
  virtual void UpdateConfigInMissive(
      const reporting::ListOfBlockedDestinations& destinations) = 0;
  virtual void UpdateEncryptionKey(
      const reporting::SignedEncryptionInfo& encryption_info) = 0;
  virtual void ReportSuccess(
      const reporting::SequenceInformation& sequence_information,
      bool force_confirm) = 0;
  virtual base::WeakPtr<MissiveClient> GetWeakPtr() = 0;

  // Returns `true` is Chromium has a valid API KEY to communicate to the
  // reporting server.
  bool has_valid_api_key() const;

  // Returns `false` initially, and `true` after `Init()` has been called.
  bool is_initialized() const;

  // Returns sequenced task runner.
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner() const;

 protected:
  // Initialize/Shutdown should be used instead.
  MissiveClient();
  virtual ~MissiveClient();

  // Sequenced task runner - must be the first member of the class.
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  SEQUENCE_CHECKER(origin_checker_);

  // Flag indicating that Chromium has a valid API key. Assumed to be `true`
  // initially, can be reset to `false` once, by `Init()`.
  bool has_valid_api_key_ = true;

  // Flag indicating that the client has been successfully initialized.
  // Assumed to be `false` initially, set to `true` by `Init()`.
  bool is_initialized_ = false;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MISSIVE_MISSIVE_CLIENT_H_
