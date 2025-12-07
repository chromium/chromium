// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/soda/soda_client_impl.h"

#include <tuple>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"

namespace soda {

SodaClientImpl::SodaClientImpl(base::FilePath library_path)
    : lib_(library_path),
      create_soda_func_(reinterpret_cast<CreateSodaFunction>(
          lib_.GetFunctionPointer("CreateExtendedSodaAsync"))),
      delete_soda_func_(reinterpret_cast<DeleteSodaFunction>(
          lib_.GetFunctionPointer("DeleteExtendedSodaAsync"))),
      add_audio_func_(reinterpret_cast<AddAudioFunction>(
          lib_.GetFunctionPointer("ExtendedAddAudio"))),
      mark_done_func_(reinterpret_cast<MarkDoneFunction>(
          lib_.GetFunctionPointer("ExtendedSodaMarkDone"))),
      soda_start_func_(reinterpret_cast<SodaStartFunction>(
          lib_.GetFunctionPointer("ExtendedSodaStart"))),
      update_recognition_context_func_(
          reinterpret_cast<UpdateRecognitionContextFunction>(
              lib_.GetFunctionPointer("UpdateRecognitionContext"))) {
  if (!lib_.is_valid()) {
    LOG(ERROR) << "SODA binary at " << library_path.value()
               << " could not be loaded.";
    LOG(ERROR) << "Error: " << lib_.GetError()->ToString();
    DCHECK(false);
  }

  // We do not need to check the |update_recognition_context_func_| since it is
  // not available in old SODA versions.
  DCHECK(create_soda_func_);
  DCHECK(delete_soda_func_);
  DCHECK(add_audio_func_);
  DCHECK(mark_done_func_);
  DCHECK(soda_start_func_);

  if (!lib_.is_valid()) {
    load_soda_result_ = LoadSodaResultValue::kBinaryInvalid;

    if (base::FeatureList::IsEnabled(media::kLogSodaLoadFailures)) {
      base::debug::DumpWithoutCrashing();
    }
  } else if (!(create_soda_func_ && delete_soda_func_ && add_audio_func_ &&
               soda_start_func_ && mark_done_func_)) {
    load_soda_result_ = LoadSodaResultValue::kFunctionPointerInvalid;
  } else {
    load_soda_result_ = LoadSodaResultValue::kSuccess;
  }

  base::UmaHistogramEnumeration("Accessibility.LiveCaption.LoadSodaResult",
                                load_soda_result_);

#if BUILDFLAG(IS_WIN)
  if (load_soda_result_ == LoadSodaResultValue::kBinaryInvalid) {
    base::UmaHistogramSparse("Accessibility.LiveCaption.LoadSodaErrorCode",
                             lib_.GetError()->code);
  }
#endif  // BUILDFLAG(IS_WIN)
}

NO_SANITIZE("cfi-icall")
SodaClientImpl::~SodaClientImpl() {
  if (load_soda_result_ != LoadSodaResultValue::kSuccess) {
    return;
  }

  if (IsInitialized()) {
    delete_soda_func_(soda_async_handle_);
  }
}

NO_SANITIZE("cfi-icall")
void SodaClientImpl::AddAudio(const char* audio_buffer, int audio_buffer_size) {
  if (load_soda_result_ != LoadSodaResultValue::kSuccess) {
    return;
  }

  add_audio_func_(soda_async_handle_, audio_buffer, audio_buffer_size);
}

NO_SANITIZE("cfi-icall")
void SodaClientImpl::MarkDone() {
  if (load_soda_result_ != LoadSodaResultValue::kSuccess) {
    return;
  }
  mark_done_func_(soda_async_handle_);
}

bool SodaClientImpl::DidAudioPropertyChange(int sample_rate,
                                            int channel_count) {
  return sample_rate != sample_rate_ || channel_count != channel_count_;
}

NO_SANITIZE("cfi-icall")
void SodaClientImpl::Reset(const SerializedSodaConfig config,
                           int sample_rate,
                           int channel_count) {
  if (load_soda_result_ != LoadSodaResultValue::kSuccess) {
    return;
  }

  if (IsInitialized()) {
    delete_soda_func_(soda_async_handle_);
  }

  soda_async_handle_ = create_soda_func_(config);
  sample_rate_ = sample_rate;
  channel_count_ = channel_count;
  is_initialized_ = true;
  soda_start_func_(soda_async_handle_);
}

NO_SANITIZE("cfi-icall")
void SodaClientImpl::UpdateRecognitionContext(
    const RecognitionContext context) {
  if (load_soda_result_ != LoadSodaResultValue::kSuccess ||
      !update_recognition_context_func_) {
    return;
  }
  update_recognition_context_func_(soda_async_handle_, context);
}

bool SodaClientImpl::IsInitialized() {
  return is_initialized_;
}

bool SodaClientImpl::BinaryLoadedSuccessfully() {
  return load_soda_result_ == LoadSodaResultValue::kSuccess;
}

}  // namespace soda
