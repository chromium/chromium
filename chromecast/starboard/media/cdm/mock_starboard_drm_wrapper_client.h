// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_CDM_MOCK_STARBOARD_DRM_WRAPPER_CLIENT_H_
#define CHROMECAST_STARBOARD_MEDIA_CDM_MOCK_STARBOARD_DRM_WRAPPER_CLIENT_H_

#include <cstdint>
#include <string>

#include "chromecast/starboard/media/cdm/starboard_drm_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

// A mock client of StarboardDrmWrapper. Constructing one can be used to
// simulate a CDM being created (due to the implementation of
// StarboardDrmWrapper::Client).
class MockStarboardDrmWrapperClient : public StarboardDrmWrapper::Client {
 public:
  MockStarboardDrmWrapperClient();

  // Disallow copy and assign.
  MockStarboardDrmWrapperClient(const MockStarboardDrmWrapperClient&) = delete;
  MockStarboardDrmWrapperClient& operator=(
      const MockStarboardDrmWrapperClient&) = delete;

  ~MockStarboardDrmWrapperClient() override;

  MOCK_METHOD(void,
              OnSessionUpdateRequest,
              (int ticket,
               StarboardDrmStatus status,
               StarboardDrmSessionRequestType type,
               std::string error_message,
               std::string session_id,
               std::vector<uint8_t> content),
              (override));

  MOCK_METHOD(void,
              OnSessionUpdated,
              (int ticket,
               StarboardDrmStatus status,
               std::string error_message,
               std::string session_id),
              (override));

  MOCK_METHOD(void,
              OnKeyStatusesChanged,
              (std::string session_id,
               std::vector<StarboardDrmKeyId> key_ids,
               std::vector<StarboardDrmKeyStatus> key_statuses),
              (override));

  MOCK_METHOD(void,
              OnCertificateUpdated,
              (int ticket,
               StarboardDrmStatus status,
               std::string error_message),
              (override));

  MOCK_METHOD(void, OnSessionClosed, (std::string session_id), (override));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_CDM_MOCK_STARBOARD_DRM_WRAPPER_CLIENT_H_
