// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <starboard/egl.h>
#include <starboard/event.h>
#include <starboard/gles.h>
#include <starboard/player.h>
#include <starboard/window.h>

#include <atomic>

#include "base/compiler_specific.h"
#include "chromecast/public/chromecast_export.h"

// Note: We combine CHROMECAST_EXPORT and WEAK_SYMBOL so that these dummy
// fallback symbols are exported from shared libraries with low precedence and
// are seamlessly overridden by strong symbols from partner Starboard libraries
// (libcast_starboard_api.so) without linker collisions or dynamic loading
// failures.
#define CHROMECAST_EXPORT_WEAK CHROMECAST_EXPORT WEAK_SYMBOL

namespace {
std::atomic<void (*)(const SbEvent*)> g_dummy_event_callback{nullptr};
}  // namespace

extern "C" {
#if SB_API_VERSION >= 15
CHROMECAST_EXPORT_WEAK int SbRunStarboardMain(int argc,
                                              char** argv,
                                              SbEventHandleCallback callback) {
  g_dummy_event_callback.store(callback, std::memory_order_release);
  if (callback) {
    SbEvent event = {};
    event.type = kSbEventTypeStart;
    callback(&event);
  }
  return 0;
}

CHROMECAST_EXPORT_WEAK void SbSystemRequestStop(int error_level) {
  if (auto* cb =
          g_dummy_event_callback.exchange(nullptr, std::memory_order_acq_rel)) {
    SbEvent event = {};
    event.type = kSbEventTypeStop;
    cb(&event);
  }
}
#else
CHROMECAST_EXPORT_WEAK int CastStarboardApiInitialize(int argc,
                                                      char** argv,
                                                      void (*callback)(void*)) {
  auto typed_cb = reinterpret_cast<void (*)(const SbEvent*)>(callback);
  g_dummy_event_callback.store(typed_cb, std::memory_order_release);
  if (typed_cb) {
    SbEvent event = {};
    event.type = kSbEventTypeStart;
    typed_cb(&event);
  }
  return 0;
}

CHROMECAST_EXPORT_WEAK void CastStarboardApiFinalize() {
  if (auto* cb =
          g_dummy_event_callback.exchange(nullptr, std::memory_order_acq_rel)) {
    SbEvent event = {};
    event.type = kSbEventTypeStop;
    cb(&event);
  }
}
#endif

CHROMECAST_EXPORT_WEAK int StarboardMain(int argc, char** argv) {
  return 0;
}

CHROMECAST_EXPORT_WEAK const SbGlesInterface* SbGetGlesInterface() {
  return nullptr;
}
CHROMECAST_EXPORT_WEAK const SbEglInterface* SbGetEglInterface() {
  return nullptr;
}

CHROMECAST_EXPORT_WEAK SbWindow SbWindowCreate(const SbWindowOptions* options) {
  return nullptr;
}

CHROMECAST_EXPORT_WEAK void* SbWindowGetPlatformHandle(SbWindow window) {
  return nullptr;
}

CHROMECAST_EXPORT_WEAK SbPlayer
SbPlayerCreate(SbWindow window,
               const SbPlayerCreationParam* creation_param,
               SbPlayerDeallocateSampleFunc sample_deallocate_func,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               SbPlayerErrorFunc player_error_func,
               void* context,
               SbDecodeTargetGraphicsContextProvider* context_provider) {
  return nullptr;
}

CHROMECAST_EXPORT_WEAK void SbPlayerSetBounds(SbPlayer player,
                                              int z_index,
                                              int x,
                                              int y,
                                              int width,
                                              int height) {}
CHROMECAST_EXPORT_WEAK void SbPlayerWriteEndOfStream(SbPlayer player,
                                                     SbMediaType stream_type) {}

CHROMECAST_EXPORT_WEAK void SbPlayerSeek(SbPlayer player,
                                         SbTime seek_to_timestamp,
                                         int ticket) {}
CHROMECAST_EXPORT_WEAK void SbPlayerSeek2(SbPlayer player,
                                          SbTime seek_to_timestamp,
                                          int ticket) {}

#if SB_API_VERSION >= 15
CHROMECAST_EXPORT_WEAK void SbPlayerWriteSamples(
    SbPlayer player,
    SbMediaType sample_type,
    const SbPlayerSampleInfo* sample_infos,
    int number_of_sample_infos) {}
CHROMECAST_EXPORT_WEAK void SbPlayerGetInfo(SbPlayer player,
                                            SbPlayerInfo* out_player_info) {}
CHROMECAST_EXPORT_WEAK void SbPlayerWriteSample2(SbPlayer player,
                                                 SbMediaType sample_type,
                                                 const void* sample_infos,
                                                 int number_of_sample_infos) {}
CHROMECAST_EXPORT_WEAK void SbPlayerGetInfo2(SbPlayer player,
                                             void* out_player_info2) {}
#else
CHROMECAST_EXPORT_WEAK void SbPlayerWriteSamples(SbPlayer player,
                                                 SbMediaType sample_type,
                                                 const void* sample_infos,
                                                 int number_of_sample_infos) {}
CHROMECAST_EXPORT_WEAK void SbPlayerGetInfo(SbPlayer player,
                                            void* out_player_info) {}
CHROMECAST_EXPORT_WEAK void SbPlayerWriteSample2(
    SbPlayer player,
    SbMediaType sample_type,
    const SbPlayerSampleInfo* sample_infos,
    int number_of_sample_infos) {}
CHROMECAST_EXPORT_WEAK void SbPlayerGetInfo2(SbPlayer player,
                                             SbPlayerInfo2* out_player_info2) {}
#endif
CHROMECAST_EXPORT_WEAK void SbPlayerSetVolume(SbPlayer player, double volume) {}
CHROMECAST_EXPORT_WEAK bool SbPlayerSetPlaybackRate(SbPlayer player,
                                                    double playback_rate) {
  return false;
}
CHROMECAST_EXPORT_WEAK void SbPlayerDestroy(SbPlayer player) {}

CHROMECAST_EXPORT_WEAK SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback) {
  return kSbDrmSystemInvalid;
}
CHROMECAST_EXPORT_WEAK void SbDrmGenerateSessionUpdateRequest(
    SbDrmSystem drm_system,
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {}
CHROMECAST_EXPORT_WEAK void SbDrmUpdateSession(SbDrmSystem drm_system,
                                               int ticket,
                                               const void* key,
                                               int key_size,
                                               const void* session_id,
                                               int session_id_size) {}
CHROMECAST_EXPORT_WEAK void SbDrmCloseSession(SbDrmSystem drm_system,
                                              const void* session_id,
                                              int session_id_size) {}
CHROMECAST_EXPORT_WEAK bool SbDrmIsServerCertificateUpdatable(
    SbDrmSystem drm_system) {
  return false;
}
CHROMECAST_EXPORT_WEAK void SbDrmUpdateServerCertificate(
    SbDrmSystem drm_system,
    int ticket,
    const void* certificate,
    int certificate_size) {}
CHROMECAST_EXPORT_WEAK const void* SbDrmGetMetrics(SbDrmSystem drm_system,
                                                   int* size) {
  return nullptr;
}
CHROMECAST_EXPORT_WEAK void SbDrmDestroySystem(SbDrmSystem drm_system) {}

CHROMECAST_EXPORT_WEAK SbMediaSupportType
SbMediaCanPlayMimeAndKeySystem(const char* mime, const char* key_system) {
  return kSbMediaSupportTypeMaybe;
}
}
