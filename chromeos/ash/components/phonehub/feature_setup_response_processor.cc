// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/feature_setup_response_processor.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/message_receiver.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

FeatureSetupResponseProcessor::FeatureSetupResponseProcessor(
    MessageReceiver* message_receiver,
    MultideviceFeatureAccessManager* multidevice_feature_access_manager)
    : message_receiver_(message_receiver),
      multidevice_feature_access_manager_(multidevice_feature_access_manager) {
  DCHECK(message_receiver_);
  DCHECK(multidevice_feature_access_manager_);

  message_receiver_->AddObserver(this);
}

FeatureSetupResponseProcessor::~FeatureSetupResponseProcessor() {
  message_receiver_->RemoveObserver(this);
}

void FeatureSetupResponseProcessor::OnFeatureSetupResponseReceived(
    proto::FeatureSetupResponse response) {
  if (!multidevice_feature_access_manager_
           ->IsCombinedSetupOperationInProgress()) {
    return;
  }

  if (response.camera_roll_setup_result() ==
          proto::FeatureSetupResult::RESULT_ERROR_ACTION_CANCELED ||
      response.notification_setup_result() ==
          proto::FeatureSetupResult::RESULT_ERROR_ACTION_CANCELED ||
      response.notification_setup_result() ==
          proto::FeatureSetupResult::RESULT_ERROR_ACTION_TIMEOUT) {
    multidevice_feature_access_manager_->SetCombinedSetupOperationStatus(
        CombinedAccessSetupOperation::Status::kOperationFailedOrCancelled);
  } else if (response.camera_roll_setup_result() ==
                 proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED &&
             response.notification_setup_result() ==
                 proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT) {
    multidevice_feature_access_manager_->SetCombinedSetupOperationStatus(
        CombinedAccessSetupOperation::Status::
            kCameraRollGrantedNotificationRejected);
  } else if (response.camera_roll_setup_result() ==
                 proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT &&
             response.notification_setup_result() ==
                 proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED) {
    multidevice_feature_access_manager_->SetCombinedSetupOperationStatus(
        CombinedAccessSetupOperation::Status::
            kCameraRollRejectedNotificationGranted);
  } else if (response.camera_roll_setup_result() ==
                 proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT ||
             response.notification_setup_result() ==
                 proto::FeatureSetupResult::RESULT_ERROR_USER_REJECT) {
    multidevice_feature_access_manager_->SetCombinedSetupOperationStatus(
        CombinedAccessSetupOperation::Status::kCompletedUserRejectedAllAccess);
  }
}

}  // namespace phonehub
}  // namespace ash
