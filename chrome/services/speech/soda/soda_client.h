// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_H_
#define CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_H_

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
  virtual ~SodaClient() = default;

  // Feeds raw audio to SODA in the form of a contiguous stream of characters.
  virtual void AddAudio(const char* audio_buffer, int audio_buffer_size) = 0;

  // Notifies the client to finish transcribing.
  virtual void MarkDone() = 0;

  // Checks whether the sample rate or channel count differs from the values
  // used to initialize the SODA instance.
  virtual bool DidAudioPropertyChange(int sample_rate, int channel_count) = 0;

  // Resets the SODA instance, initializing it with the provided config.
  virtual void Reset(const SerializedSodaConfig config,
                     int sample_rate,
                     int channel_count) = 0;

  // Updates the recognition context for the current SODA instance.
  virtual void UpdateRecognitionContext(const RecognitionContext context) = 0;

  // Returns a flag indicating whether the client has been initialized.
  virtual bool IsInitialized() = 0;

  // Returns a flag indicating whether the binary was loaded successfully.
  virtual bool BinaryLoadedSuccessfully() = 0;
};

}  // namespace soda

#endif  // CHROME_SERVICES_SPEECH_SODA_SODA_CLIENT_H_
