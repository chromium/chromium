// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_DEVICE_SOURCE_H_
#define COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_DEVICE_SOURCE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/sharing_message/sharing_device_source.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockSharingDeviceSource : public SharingDeviceSource {
 public:
  MockSharingDeviceSource();
  MockSharingDeviceSource(const MockSharingDeviceSource&) = delete;
  MockSharingDeviceSource& operator=(const MockSharingDeviceSource&) = delete;
  ~MockSharingDeviceSource() override;

  MOCK_METHOD0(IsReady, bool());

  MOCK_METHOD1(GetDeviceByGuid,
               std::optional<SharingTargetDeviceInfo>(const std::string& guid));

  MOCK_METHOD1(
      GetDeviceCandidates,
      std::vector<SharingTargetDeviceInfo>(
          sync_pb::SharingSpecificFields::EnabledFeatures required_feature));

  void MaybeRunReadyCallbacksForTesting() { MaybeRunReadyCallbacks(); }
};

#endif  // COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_DEVICE_SOURCE_H_
