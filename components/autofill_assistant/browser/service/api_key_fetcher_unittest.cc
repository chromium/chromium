// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/api_key_fetcher.h"

#include "base/command_line.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Eq;

TEST(ApiKeyFetcherTest, GetAPIKeyCommandLineOverride) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantServerKey, "fake_key");

  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::STABLE),
              Eq("fake_key"));
  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::UNKNOWN),
              Eq("fake_key"));
  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::CANARY),
              Eq("fake_key"));
  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::DEV),
              Eq("fake_key"));
  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::BETA),
              Eq("fake_key"));
}

TEST(ApiKeyFetcherTest, GetAPIKeyReturnsStableAndNonstableKeys) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kAutofillAssistantServerKey);
  // Only run tests on Google Chrome builds. This intentionally does not gate
  // the test with a precompiler definition, because a change to
  // |IsGoogleChromeAPIKeyUsed| would go unnoticed by us.
  if (!google_apis::IsGoogleChromeAPIKeyUsed()) {
    return;
  }

  std::string api_key_stable = google_apis::GetAPIKey();
  std::string api_key_nonstable = google_apis::GetNonStableAPIKey();

  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::STABLE),
              Eq(api_key_stable));
  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::UNKNOWN),
              Eq(api_key_nonstable));
  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::CANARY),
              Eq(api_key_nonstable));
  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::DEV),
              Eq(api_key_nonstable));
  EXPECT_THAT(ApiKeyFetcher().GetAPIKey(version_info::Channel::BETA),
              Eq(api_key_nonstable));
}

}  // namespace
}  // namespace autofill_assistant
