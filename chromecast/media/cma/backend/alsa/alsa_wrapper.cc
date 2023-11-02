// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/alsa_wrapper.h"

#include "chromecast/media/cma/backend/audio_buildflags.h"

namespace chromecast {
namespace media {

#if BUILDFLAG(MEDIA_CLOCK_MONOTONIC_RAW)
const int kAlsaTstampTypeMonotonicRaw =
    static_cast<int>(SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW);
#else
const int kAlsaTstampTypeMonotonicRaw = 0;
#endif  // BUILDFLAG(MEDIA_CLOCK_MONOTONIC_RAW)

AlsaWrapper::AlsaWrapper() {
}

AlsaWrapper::~AlsaWrapper() {
}

int AlsaWrapper::PcmPause(snd_pcm_t* handle, int enable) {
  return snd_pcm_pause(handle, enable);
}

int AlsaWrapper::PcmStatusMalloc(snd_pcm_status_t** ptr) {
  return snd_pcm_status_malloc(ptr);
}

void AlsaWrapper::PcmStatusFree(snd_pcm_status_t* obj) {
  snd_pcm_status_free(obj);
}

int AlsaWrapper::PcmStatus(snd_pcm_t* handle, snd_pcm_status_t* status) {
  return snd_pcm_status(handle, status);
}

snd_pcm_sframes_t AlsaWrapper::PcmStatusGetDelay(const snd_pcm_status_t* obj) {
  return snd_pcm_status_get_delay(obj);
}

snd_pcm_uframes_t AlsaWrapper::PcmStatusGetAvail(const snd_pcm_status_t* obj) {
  return snd_pcm_status_get_avail(obj);
}

void AlsaWrapper::PcmStatusGetHtstamp(const snd_pcm_status_t* obj,
                                      snd_htimestamp_t* ptr) {
  snd_pcm_status_get_htstamp(obj, ptr);
}

snd_pcm_state_t AlsaWrapper::PcmStatusGetState(const snd_pcm_status_t* obj) {
  return snd_pcm_status_get_state(obj);
}

int AlsaWrapper::PcmHwParamsCurrent(snd_pcm_t* handle,
                                    snd_pcm_hw_params_t* params) {
  return snd_pcm_hw_params_current(handle, params);
}

int AlsaWrapper::PcmHwParamsCanPause(const snd_pcm_hw_params_t* params) {
  return snd_pcm_hw_params_can_pause(params);
}

int AlsaWrapper::PcmHwParamsTestRate(snd_pcm_t* handle,
                                     snd_pcm_hw_params_t* params,
                                     unsigned int rate,
                                     int dir) {
  return snd_pcm_hw_params_test_rate(handle, params, rate, dir);
}

int AlsaWrapper::PcmSwParamsSetTstampMode(snd_pcm_t* handle,
                                          snd_pcm_sw_params_t* obj,
                                          snd_pcm_tstamp_t val) {
  return snd_pcm_sw_params_set_tstamp_mode(handle, obj, val);
}

int AlsaWrapper::PcmSwParamsSetTstampType(snd_pcm_t* handle,
                                          snd_pcm_sw_params_t* obj,
                                          int val) {
#if BUILDFLAG(MEDIA_CLOCK_MONOTONIC_RAW)
  return snd_pcm_sw_params_set_tstamp_type(
      handle, obj, static_cast<snd_pcm_tstamp_type_t>(val));
#else
  return 0;
#endif  // BUILDFLAG(MEDIA_CLOCK_MONOTONIC_RAW)
}

}  // namespace media
}  // namespace chromecast
