// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_H_
#define CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_H_

#include "base/scoped_native_library.h"
#include "chrome/services/speech/soda/soda_async_impl.h"

namespace soda {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LoadSodaResultValue {
  kUnknown = 0,
  kSuccess = 1,
  kBinaryInvalid = 2,
  kFunctionPointerInvalid = 3,
  kMaxValue = kFunctionPointerInvalid,
};

// The client that wraps the plain C-style interface between Chrome and the
// Speech On-Device API (SODA). Changes to the interface must be backwards
// compatible and reflected in the Google3-side definition.
class SodaClient {
 public:
  // Takes in the fully-qualified path to the SODA binary.
  explicit SodaClient(base::FilePath library_path);
  ~SodaClient();

  // Feeds raw audio to SODA in the form of a contiguous stream of characters.
  void AddAudio(const char* audio_buffer, int audio_buffer_size);

  // Checks whether the sample rate or channel count differs from the values
  // used to initialize the SODA instance.
  bool DidAudioPropertyChange(int sample_rate, int channel_count);

  // Resets the SODA instance, initializing it with the provided config.
  void Reset(const SerializedSodaConfig config,
             int sample_rate,
             int channel_count);

  // Returns a flag indicating whether the client has been initialized.
  bool IsInitialized() { return is_initialized_; }

  bool BinaryLoadedSuccessfully() {
    return load_soda_result_ == LoadSodaResultValue::kSuccess;
  }

 private:
  base::ScopedNativeLibrary lib_;

  typedef void* (*CreateSodaFunction)(SerializedSodaConfig);
  CreateSodaFunction create_soda_func_;

  typedef void (*DeleteSodaFunction)(void*);
  DeleteSodaFunction delete_soda_func_;

  typedef void (*AddAudioFunction)(void*, const char*, int);
  AddAudioFunction add_audio_func_;

  typedef void (*SodaStartFunction)(void*);
  SodaStartFunction soda_start_func_;

  // An opaque handle to the SODA async instance.
  void* soda_async_handle_;

  LoadSodaResultValue load_soda_result_ = LoadSodaResultValue::kUnknown;
  bool is_initialized_;
  int sample_rate_;
  int channel_count_;

  DISALLOW_COPY_AND_ASSIGN(SodaClient);
};

}  // namespace soda

#endif  // CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_H_
