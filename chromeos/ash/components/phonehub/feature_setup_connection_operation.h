// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_SETUP_CONNECTION_OPERATION_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_SETUP_CONNECTION_OPERATION_H_

#include <optional>
#include <ostream>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace ash {
namespace phonehub {

// Implements the connection establish flow. This flow involves:
// (1) Creating a connection to the phone if one does not already exist.
// (2) Continues to appropriate next step, e.g. setup screen, notification
// setup,
//     combimed setup, and etc.
//
// If an instance of this class exists, the flow continues until the status
// changes to a "final" status (i.e., a success or a fatal error). To cancel the
// ongoing setup operation, simply delete the instance of this class.
class FeatureSetupConnectionOperation {
 public:
  // Note: Numerical values are from
  // multidevice_permissions_setup_dialog.js. We only use values that are
  // meaningful in this flow.
  enum class Status {
    // Trying to establish connection to the phone.
    kConnecting = 1,

    // The connecting process time out. User can try again.
    kTimedOutConnecting = 2,

    // Connection lost. Users can reconnect.
    kConnectionLost = 3,

    // The flow is finished and is in final status.
    kCompletedSuccessfully = 5,

    // Connection has been established and proceed to next steps.
    kConnected = 11,
    kMaxValue = kConnected
  };

  static bool IsFinalStatus(Status status);

  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnFeatureSetupConnectionStatusChange(Status new_status) = 0;
  };

  FeatureSetupConnectionOperation(const FeatureSetupConnectionOperation&) =
      delete;
  FeatureSetupConnectionOperation& operator=(
      const FeatureSetupConnectionOperation&) = delete;

  virtual ~FeatureSetupConnectionOperation();

 private:
  friend class MultideviceFeatureAccessManager;

  FeatureSetupConnectionOperation(Delegate* delegate,
                                  base::OnceClosure destructor_callback);

  void NotifyFeatureSetupConnectionStatusChanged(Status new_status);

  std::optional<Status> current_status_;
  const raw_ptr<Delegate> delegate_;
  base::OnceClosure destructor_callback_;
};

std::ostream& operator<<(std::ostream& stream,
                         FeatureSetupConnectionOperation::Status status);

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_SETUP_CONNECTION_OPERATION_H_
