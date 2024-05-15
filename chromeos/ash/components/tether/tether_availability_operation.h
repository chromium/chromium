// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_AVAILABILITY_OPERATION_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_AVAILABILITY_OPERATION_H_

#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/tether/message_transfer_operation.h"
#include "chromeos/ash/components/tether/scanned_device_info.h"
#include "chromeos/ash/components/tether/tether_host.h"

namespace ash::tether {

class ConnectionPreserver;
class MessageWrapper;
class TetherHostResponseRecorder;

// Operation used to determine if a device has support for tether.
// This operation sends a TetherAvailabilityRequest to the connected device
// once an authenticated channel has been established; once a response has been
// received, TetherAvailabilityOperation alerts caller of if the device can
// provide a tethering connection.
class TetherAvailabilityOperation : public MessageTransferOperation {
 public:
  using OnTetherAvailabilityOperationFinishedCallback =
      base::OnceCallback<void(std::optional<ScannedDeviceInfo>)>;

  class Initializer {
   public:
    Initializer(
        raw_ptr<HostConnection::Factory> host_connection_factory,
        raw_ptr<TetherHostResponseRecorder> tether_host_response_recorder,
        raw_ptr<ConnectionPreserver> connection_preserver);

    virtual std::unique_ptr<TetherAvailabilityOperation> Initialize(
        const TetherHost& tether_host,
        OnTetherAvailabilityOperationFinishedCallback callback);

    virtual ~Initializer();

   private:
    raw_ptr<HostConnection::Factory> host_connection_factory_;
    raw_ptr<TetherHostResponseRecorder> tether_host_response_recorder_;
    raw_ptr<ConnectionPreserver> connection_preserver_;
  };

  TetherAvailabilityOperation(const TetherAvailabilityOperation&) = delete;
  TetherAvailabilityOperation& operator=(const TetherAvailabilityOperation&) =
      delete;

  TetherAvailabilityOperation(
      const TetherHost& tether_host,
      OnTetherAvailabilityOperationFinishedCallback on_operation_finished,
      raw_ptr<HostConnection::Factory> host_connection_factory,
      TetherHostResponseRecorder* tether_host_response_recorder,
      ConnectionPreserver* connection_preserver);

  ~TetherAvailabilityOperation() override;

 protected:
  // MessageTransferOperation:
  void OnDeviceAuthenticated() override;
  void OnMessageReceived(
      std::unique_ptr<MessageWrapper> message_wrapper) override;
  void OnOperationFinished() override;
  MessageType GetMessageTypeForConnection() override;

 private:
  TetherHost tether_host_;

  friend class TetherAvailabilityOperationTest;
  FRIEND_TEST_ALL_PREFIXES(TetherAvailabilityOperationTest,
                           DevicesArePrioritizedDuringConstruction);
  FRIEND_TEST_ALL_PREFIXES(TetherAvailabilityOperationTest,
                           RecordsResponseDuration);
  FRIEND_TEST_ALL_PREFIXES(TetherAvailabilityOperationTest, ErrorResponses);
  FRIEND_TEST_ALL_PREFIXES(TetherAvailabilityOperationTest,
                           NotificationsDisabled);
  FRIEND_TEST_ALL_PREFIXES(TetherAvailabilityOperationTest,
                           NotificationsDisabledWithNotificationChannel);
  FRIEND_TEST_ALL_PREFIXES(TetherAvailabilityOperationTest, TetherAvailable);
  FRIEND_TEST_ALL_PREFIXES(TetherAvailabilityOperationTest,
                           LastProvisioningFailed);
  FRIEND_TEST_ALL_PREFIXES(TetherAvailabilityOperationTest, SetupRequired);
  FRIEND_TEST_ALL_PREFIXES(TetherAvailabilityOperationTest,
                           TestMultipleDevices);

  void SetTestDoubles(base::Clock* clock_for_test,
                      scoped_refptr<base::TaskRunner> test_task_runner);
  void RecordTetherAvailabilityResponseDuration(const std::string device_id);

  raw_ptr<TetherHostResponseRecorder> tether_host_response_recorder_;
  raw_ptr<ConnectionPreserver> connection_preserver_;
  raw_ptr<base::Clock> clock_;
  scoped_refptr<base::TaskRunner> task_runner_;

  std::optional<ScannedDeviceInfo> scanned_device_info_result_;
  OnTetherAvailabilityOperationFinishedCallback on_operation_finished_;
  std::optional<base::Time> tether_availability_request_start_time_;

  base::WeakPtrFactory<TetherAvailabilityOperation> weak_ptr_factory_{this};
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_AVAILABILITY_OPERATION_H_
