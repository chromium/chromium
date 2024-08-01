// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_API_WRAPPER_BASE_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_API_WRAPPER_BASE_H_

#include <starboard/media.h>
#include <starboard/player.h>

#include <vector>

#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {
namespace media {

// Implements the functions of StarboardApiWrapper that are common across
// different starboard versions. This is still an abstract class, as it does not
// implement all of StarboardApiWrapper's functions.
class StarboardApiWrapperBase : public StarboardApiWrapper {
 public:
  StarboardApiWrapperBase();
  ~StarboardApiWrapperBase() override;

  // StarboardApiWrapper impl:
  bool EnsureInitialized() override;
  void* CreatePlayer(
      const StarboardPlayerCreationParam* creation_param,
      const StarboardPlayerCallbackHandler* callback_handler) override;
  void SetPlayerBounds(void* player,
                       int z_index,
                       int x,
                       int y,
                       int width,
                       int height) override;
  void WriteSample(void* player,
                   StarboardMediaType type,
                   StarboardSampleInfo* sample_infos,
                   int sample_infos_count) override;
  void WriteEndOfStream(void* player, StarboardMediaType type) override;
  void SetVolume(void* player, double volume) override;
  bool SetPlaybackRate(void* player, double playback_rate) override;
  void DestroyPlayer(void* player) override;
  void* CreateDrmSystem(
      const char* key_system,
      const StarboardDrmSystemCallbackHandler* callback_handler) override;
  void DrmGenerateSessionUpdateRequest(void* drm_system,
                                       int ticket,
                                       const char* type,
                                       const void* initialization_data,
                                       int initialization_data_size) override;
  void DrmUpdateSession(void* drm_system,
                        int ticket,
                        const void* key,
                        int key_size,
                        const void* session_id,
                        int session_id_size) override;
  void DrmCloseSession(void* drm_system,
                       const void* session_id,
                       int session_id_size) override;
  void DrmUpdateServerCertificate(void* drm_system,
                                  int ticket,
                                  const void* certificate,
                                  int certificate_size) override;
  bool DrmIsServerCertificateUpdatable(void* drm_system) override;
  void DrmDestroySystem(void* drm_system) override;

  StarboardMediaSupportType CanPlayMimeAndKeySystem(
      const char* mime,
      const char* key_system) override;

 private:
  // Converts StarboardSampleInfo to SbPlayerSampleInfo. `side_data` is used to
  // store side data for this sample, since that data must outlive this function
  // call. Same for `drm_info` and `subsample_mappings`.
  //
  // `side_data` and `subsample_mappings` should be empty vectors. They will be
  // populated as necessary, so that they can outlive the later call to
  // SbPlayerWriteSample.
  SbPlayerSampleInfo ToSbPlayerSampleInfo(
      const StarboardSampleInfo& in_info,
      std::vector<SbPlayerSampleSideData>& side_data,
      SbDrmSampleInfo& drm_info,
      std::vector<SbDrmSubSampleMapping>& subsample_mappings);

  // Converts the version-agnostic StarboardPlayerCreationParam to starboard's
  // SbPlayerCreationParam.
  virtual SbPlayerCreationParam ToSbPlayerCreationParam(
      const StarboardPlayerCreationParam& in_param,
      void* drm_system) = 0;

  // Converts from cast's version-agnostic struct to starboard's version.
  virtual SbMediaVideoSampleInfo ToSbMediaVideoSampleInfo(
      const StarboardVideoSampleInfo& in_video_info) = 0;

  // Converts from cast's version-agnostic struct to starboard's version.
  virtual SbMediaAudioSampleInfo ToSbMediaAudioSampleInfo(
      const StarboardAudioSampleInfo& in_audio_info) = 0;

  // Calls the relevant starboard version's function to write samples (e.g.
  // SbPlayerWriteSamples or SbPlayerWriteSample2).
  virtual void CallWriteSamples(SbPlayer player,
                                SbMediaType sample_type,
                                const SbPlayerSampleInfo* sample_infos,
                                int number_of_sample_infos) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_API_WRAPPER_BASE_H_
