// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_H_
#define CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "chrome/services/speech/soda/soda_async_impl.h"

namespace soda {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(LoadSodaResultValue)
enum class LoadSodaResultValue {
  kUnknown = 0,
  kSuccess = 1,
  kBinaryInvalid = 2,
  kFunctionPointerInvalid = 3,
  kMaxValue = kFunctionPointerInvalid,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:LoadSodaResult)

// The client that wraps the plain C-style interface between Chrome and the
// Speech On-Device API (SODA). Changes to the interface must be backwards
// compatible and reflected in the Google3-side definition.
class SodaClient {
 public:
  // Takes in the fully-qualified path to the SODA binary.
  explicit SodaClient(base::FilePath library_path);

  SodaClient(const SodaClient&) = delete;
  SodaClient& operator=(const SodaClient&) = delete;

  ~SodaClient();

  // Feeds raw audio to SODA in the form of a contiguous stream of characters.
  void AddAudio(const char* audio_buffer, int audio_buffer_size);

  // Notifies the client to finish transcribing.
  void MarkDone();

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

  typedef void (*MarkDoneFunction)(void*);
  MarkDoneFunction mark_done_func_;

  typedef void (*SodaStartFunction)(void*);
  SodaStartFunction soda_start_func_;

  // An opaque handle to the SODA async instance. While this class owns this
  // handle, the handle is instantiated and deleted by the SODA library, so the
  // pointer may dangle after DeleteExtendedSodaAsync is called.
  raw_ptr<void, DisableDanglingPtrDetection> soda_async_handle_;

  LoadSodaResultValue load_soda_result_ = LoadSodaResultValue::kUnknown;
  bool is_initialized_;
  int sample_rate_;
  int channel_count_;
};

}  // namespace soda

#endif  // CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_H_
