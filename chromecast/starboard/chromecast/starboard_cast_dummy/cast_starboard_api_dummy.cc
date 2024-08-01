// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <starboard/egl.h>
#include <starboard/event.h>
#include <starboard/gles.h>
#include <starboard/player.h>
#include <starboard/window.h>

#if SB_API_VERSION < 15
#include <cast_starboard_api.h>
#endif  // SB_API_VERSION < 15

#if SB_API_VERSION >= 15
int SbRunStarboardMain(int argc, char** argv, SbEventHandleCallback callback) {
  SbEvent* fake_start = new SbEvent;
  fake_start->type = kSbEventTypeStart;
  callback(fake_start);
  return 0;
}
#else  // SB_API_VERSION >= 15
int StarboardMain(int argc, char** argv) {
  return 0;
}

int CastStarboardApiInitialize(int argc,
                               char** argv,
                               SbEventHandleCB callback) {
  SbEvent* fake_start = new SbEvent;
  fake_start->type = kSbEventTypeStart;
  callback(fake_start);
  return 0;
}

void CastStarboardApiFinalize() {}

#endif  // SB_API_VERSION >= 15

const SbGlesInterface* SbGetGlesInterface() {
  return nullptr;
}
const SbEglInterface* SbGetEglInterface() {
  return nullptr;
}

SbWindow SbWindowCreate(const SbWindowOptions* options) {
  return nullptr;
}

void* SbWindowGetPlatformHandle(SbWindow window) {
  return nullptr;
}

SbPlayer SbPlayerCreate(
    SbWindow window,
    const SbPlayerCreationParam* creation_param,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    SbDecodeTargetGraphicsContextProvider* context_provider) {
  return nullptr;
}

void SbPlayerSetBounds(SbPlayer player,
                       int z_index,
                       int x,
                       int y,
                       int width,
                       int height) {}
void SbPlayerWriteEndOfStream(SbPlayer player, SbMediaType stream_type) {}
#if SB_API_VERSION >= 15
void SbPlayerSeek(SbPlayer player, SbTime seek_to_timestamp, int ticket) {}
void SbPlayerWriteSamples(SbPlayer player,
                          SbMediaType sample_type,
                          const SbPlayerSampleInfo* sample_infos,
                          int number_of_sample_infos) {}
void SbPlayerGetInfo(SbPlayer player, SbPlayerInfo* out_player_info2) {}
#else   // SB_API_VERSION >= 15
void SbPlayerSeek2(SbPlayer player, SbTime seek_to_timestamp, int ticket) {}
void SbPlayerWriteSample2(SbPlayer player,
                          SbMediaType sample_type,
                          const SbPlayerSampleInfo* sample_infos,
                          int number_of_sample_infos) {}
void SbPlayerGetInfo2(SbPlayer player, SbPlayerInfo2* out_player_info2) {}
#endif  // SB_API_VERSION >= 15
void SbPlayerSetVolume(SbPlayer player, double volume) {}
bool SbPlayerSetPlaybackRate(SbPlayer player, double playback_rate) {
  return false;
}
void SbPlayerDestroy(SbPlayer player) {}

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback) {
  return kSbDrmSystemInvalid;
}
void SbDrmGenerateSessionUpdateRequest(SbDrmSystem drm_system,
                                       int ticket,
                                       const char* type,
                                       const void* initialization_data,
                                       int initialization_data_size) {}
void SbDrmUpdateSession(SbDrmSystem drm_system,
                        int ticket,
                        const void* key,
                        int key_size,
                        const void* session_id,
                        int session_id_size) {}
void SbDrmCloseSession(SbDrmSystem drm_system,
                       const void* session_id,
                       int session_id_size) {}
bool SbDrmIsServerCertificateUpdatable(SbDrmSystem drm_system) {
  return false;
}
void SbDrmUpdateServerCertificate(SbDrmSystem drm_system,
                                  int ticket,
                                  const void* certificate,
                                  int certificate_size) {}
const void* SbDrmGetMetrics(SbDrmSystem drm_system, int* size) {
  return nullptr;
}
void SbDrmDestroySystem(SbDrmSystem drm_system) {}

SbMediaSupportType SbMediaCanPlayMimeAndKeySystem(const char* mime,
                                                  const char* key_system) {
  return kSbMediaSupportTypeMaybe;
}
