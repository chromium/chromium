// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using TtsManifestTest = ChromeManifestTest;

namespace errors = manifest_errors;

TEST_F(TtsManifestTest, TtsEngine) {
  std::string error_invalid_sample_rate_range = base::StringPrintf(
      errors::kInvalidTtsSampleRateRange, media::limits::kMinSampleRate,
      media::limits::kMaxSampleRate);
  std::string error_invalid_buffer_size_range =
      base::StringPrintf(errors::kInvalidTtsBufferSizeRange, 1,
                         media::limits::kMaxSamplesPerPacket);

  Testcase testcases[] = {
      Testcase("tts_engine_invalid_voices_1.json", errors::kInvalidTts),
      Testcase("tts_engine_invalid_voices_2.json", errors::kInvalidTtsVoices),
      Testcase("tts_engine_invalid_voices_3.json", errors::kInvalidTtsVoices),
      Testcase("tts_engine_invalid_voices_4.json",
               errors::kInvalidTtsVoicesVoiceName),
      Testcase("tts_engine_invalid_voices_5.json",
               errors::kInvalidTtsVoicesLang),
      Testcase("tts_engine_invalid_voices_6.json",
               errors::kInvalidTtsVoicesLang),
      Testcase("tts_engine_invalid_voices_7.json",
               errors::kInvalidTtsVoicesEventTypes),
      Testcase("tts_engine_invalid_voices_8.json",
               errors::kInvalidTtsVoicesEventTypes),

      Testcase("tts_engine_invalid_sample_rate_1.json",
               errors::kInvalidTtsSampleRateFormat),
      Testcase("tts_engine_invalid_sample_rate_2.json",
               error_invalid_sample_rate_range),
      Testcase("tts_engine_invalid_sample_rate_3.json",
               error_invalid_sample_rate_range),
      Testcase("tts_engine_invalid_sample_rate_4.json",
               errors::kInvalidTtsRequiresSampleRateAndBufferSize),

      Testcase("tts_engine_invalid_buffer_size_1.json",
               errors::kInvalidTtsBufferSizeFormat),
      Testcase("tts_engine_invalid_buffer_size_2.json",
               error_invalid_buffer_size_range),
      Testcase("tts_engine_invalid_buffer_size_3.json",
               error_invalid_buffer_size_range),
      Testcase("tts_engine_invalid_buffer_size_4.json",
               errors::kInvalidTtsRequiresSampleRateAndBufferSize),
  };
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);

  LoadAndExpectSuccess("tts_engine_valid_voices.json");
  LoadAndExpectSuccess("tts_engine_valid_sample_rate_buffer_size.json");
}

}  // namespace extensions
