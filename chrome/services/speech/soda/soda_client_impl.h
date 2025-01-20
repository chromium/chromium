// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_IMPL_H_
#define CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "chrome/services/speech/soda/soda_async_impl.h"
#include "chrome/services/speech/soda/soda_client.h"

namespace soda {

// Implementation of the SodaClient.
class SodaClientImpl : public SodaClient {
 public:
  // Takes in the fully-qualified path to the SODA binary.
  explicit SodaClientImpl(base::FilePath library_path);

  SodaClientImpl(const SodaClientImpl&) = delete;
  SodaClientImpl& operator=(const SodaClientImpl&) = delete;

  ~SodaClientImpl() override;

  // SodaClient:
  void AddAudio(const char* audio_buffer, int audio_buffer_size) override;
  void MarkDone() override;
  bool DidAudioPropertyChange(int sample_rate, int channel_count) override;
  void Reset(const SerializedSodaConfig config,
             int sample_rate,
             int channel_count) override;
  void UpdateRecognitionContext(const RecognitionContext context) override;
  bool IsInitialized() override;
  bool BinaryLoadedSuccessfully() override;

 private:
  base::ScopedNativeLibrary lib_;

  typedef void* (*CreateSodaFunction)(SerializedSodaConfig);
  CreateSodaFunction create_soda_func_;

  typedef void (*DeleteSodaFunction)(void*);
  DeleteSodaFunction delete_soda_func_;

  typedef void (*AddAudioFunction)(void*, const char*, int);
  AddAudioFunction add_audio_func_;

  typedef void (*MarkDoneFunction)(void*);
  MarkDoneFunction mark_done_func_;

  typedef void (*SodaStartFunction)(void*);
  SodaStartFunction soda_start_func_;

  typedef void (*UpdateRecognitionContextFunction)(void*, RecognitionContext);
  UpdateRecognitionContextFunction update_recognition_context_func_;

  // An opaque handle to the SODA async instance. While this class owns this
  // handle, the handle is instantiated and deleted by the SODA library, so the
  // pointer may dangle after DeleteExtendedSodaAsync is called.
  raw_ptr<void, DisableDanglingPtrDetection> soda_async_handle_;

  LoadSodaResultValue load_soda_result_ = LoadSodaResultValue::kUnknown;
  bool is_initialized_ = false;
  int sample_rate_ = 0;
  int channel_count_ = 0;
};

}  // namespace soda

#endif  // CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_IMPL_H_
