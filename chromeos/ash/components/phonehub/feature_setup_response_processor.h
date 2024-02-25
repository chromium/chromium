// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_SETUP_RESPONSE_PROCESSOR_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_SETUP_RESPONSE_PROCESSOR_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/message_receiver.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

class MultideviceFeatureAccessManager;

class FeatureSetupResponseProcessor : public MessageReceiver::Observer {
 public:
  FeatureSetupResponseProcessor(
      MessageReceiver* message_receiver,
      MultideviceFeatureAccessManager* multidevice_feature_access_manager);

  ~FeatureSetupResponseProcessor() override;

  FeatureSetupResponseProcessor(const FeatureSetupResponseProcessor&) = delete;
  FeatureSetupResponseProcessor& operator=(
      const FeatureSetupResponseProcessor&) = delete;

 private:
  friend class FeatureSetupResponseProcessorTest;

  // MessageReceiver::Observer:
  void OnFeatureSetupResponseReceived(
      proto::FeatureSetupResponse response) override;

  raw_ptr<MessageReceiver> message_receiver_;
  raw_ptr<MultideviceFeatureAccessManager> multidevice_feature_access_manager_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_SETUP_RESPONSE_PROCESSOR_H_
