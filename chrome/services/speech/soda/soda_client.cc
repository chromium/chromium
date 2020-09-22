// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/soda/soda_client.h"

namespace soda {

SodaClient::SodaClient(base::FilePath library_path)
    : lib_(library_path),
      create_soda_func_(reinterpret_cast<CreateSodaFunction>(
          lib_.GetFunctionPointer("CreateSodaAsync"))),
      delete_soda_func_(reinterpret_cast<DeleteSodaFunction>(
          lib_.GetFunctionPointer("DeleteSodaAsync"))),
      add_audio_func_(reinterpret_cast<AddAudioFunction>(
          lib_.GetFunctionPointer("AddAudio"))),
      is_initialized_(false),
      sample_rate_(0),
      channel_count_(0) {}

SodaClient::~SodaClient() {
  DCHECK(soda_async_handle_);
  DCHECK(delete_soda_func_);
  if (IsInitialized())
    delete_soda_func_(soda_async_handle_);
}

void SodaClient::AddAudio(const char* audio_buffer, int audio_buffer_size) {
  DCHECK(soda_async_handle_);
  DCHECK(add_audio_func_);
  add_audio_func_(soda_async_handle_, audio_buffer, audio_buffer_size);
}

bool SodaClient::DidAudioPropertyChange(int sample_rate, int channel_count) {
  return sample_rate != sample_rate_ || channel_count != channel_count_;
}

void SodaClient::Reset(const SodaConfig config) {
  DCHECK(soda_async_handle_);
  DCHECK(create_soda_func_);
  DCHECK(delete_soda_func_);

  if (IsInitialized()) {
    delete_soda_func_(soda_async_handle_);
  }

  soda_async_handle_ = create_soda_func_(config);
  sample_rate_ = config.sample_rate;
  channel_count_ = config.channel_count;
  is_initialized_ = true;
}

}  // namespace soda
