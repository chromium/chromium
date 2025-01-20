// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SODA_MOCK_SODA_CLIENT_H_
#define CHROME_SERVICES_SPEECH_SODA_MOCK_SODA_CLIENT_H_

#include "chrome/services/speech/soda/soda_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace soda {

class MockSodaClient : public SodaClient {
 public:
  MockSodaClient();
  MockSodaClient(const MockSodaClient&) = delete;
  MockSodaClient& operator=(const MockSodaClient&) = delete;
  ~MockSodaClient() override;

  MOCK_METHOD(void,
              AddAudio,
              (const char* audio_buffer, int audio_buffer_size),
              (override));
  MOCK_METHOD(void, MarkDone, (), (override));
  MOCK_METHOD(bool,
              DidAudioPropertyChange,
              (int sample_rate, int channel_count),
              (override));
  MOCK_METHOD(void,
              Reset,
              (const SerializedSodaConfig config,
               int sample_rate,
               int channel_count),
              (override));
  MOCK_METHOD(void,
              UpdateRecognitionContext,
              (const RecognitionContext context),
              (override));
  MOCK_METHOD(bool, IsInitialized, (), (override));
  MOCK_METHOD(bool, BinaryLoadedSuccessfully, (), (override));
};

}  // namespace soda

#endif  // CHROME_SERVICES_SPEECH_SODA_MOCK_SODA_CLIENT_H_
