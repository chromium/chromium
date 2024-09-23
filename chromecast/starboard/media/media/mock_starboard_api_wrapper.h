// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_MOCK_STARBOARD_API_WRAPPER_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_MOCK_STARBOARD_API_WRAPPER_H_

#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

class MockStarboardApiWrapper : public StarboardApiWrapper {
 public:
  MockStarboardApiWrapper();
  ~MockStarboardApiWrapper() override;

  MOCK_METHOD(bool, EnsureInitialized, (), (override));

  MOCK_METHOD(void*,
              CreatePlayer,
              (const StarboardPlayerCreationParam* creation_param,
               const StarboardPlayerCallbackHandler* callback_handler),
              (override));
  MOCK_METHOD(void,
              SetPlayerBounds,
              (void* player, int z_index, int x, int y, int width, int height),
              (override));
  MOCK_METHOD(void,
              SeekTo,
              (void* player, int64_t time, int seek_ticket),
              (override));
  MOCK_METHOD(void,
              WriteSample,
              (void* player,
               StarboardMediaType type,
               StarboardSampleInfo* sample_infos,
               int sample_infos_count),
              (override));
  MOCK_METHOD(void,
              WriteEndOfStream,
              (void* player, StarboardMediaType type),
              (override));
  MOCK_METHOD(void,
              GetPlayerInfo,
              (void* player, StarboardPlayerInfo* player_info),
              (override));
  MOCK_METHOD(void, SetVolume, (void* player, double volume), (override));
  MOCK_METHOD(bool,
              SetPlaybackRate,
              (void* player, double playback_rate),
              (override));
  MOCK_METHOD(void, DestroyPlayer, (void* player), (override));

  MOCK_METHOD(void*,
              CreateDrmSystem,
              (const char* key_system,
               const StarboardDrmSystemCallbackHandler* callback_handler),
              (override));
  MOCK_METHOD(void,
              DrmGenerateSessionUpdateRequest,
              (void* drm_system,
               int ticket,
               const char* type,
               const void* initialization_data,
               int initialization_data_size),
              (override));
  MOCK_METHOD(void,
              DrmUpdateSession,
              (void* drm_system,
               int ticket,
               const void* key,
               int key_size,
               const void* session_id,
               int session_id_size),
              (override));
  MOCK_METHOD(void,
              DrmCloseSession,
              (void* drm_system, const void* session_id, int session_id_size),
              (override));
  MOCK_METHOD(void,
              DrmUpdateServerCertificate,
              (void* drm_system,
               int ticket,
               const void* certificate,
               int certificate_size),
              (override));
  MOCK_METHOD(bool,
              DrmIsServerCertificateUpdatable,
              (void* drm_system),
              (override));
  MOCK_METHOD(void, DrmDestroySystem, (void* drm_system), (override));
  MOCK_METHOD(StarboardMediaSupportType,
              CanPlayMimeAndKeySystem,
              (const char* mime, const char* key_system),
              (override));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_MOCK_STARBOARD_API_WRAPPER_H_
