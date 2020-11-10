// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/server_url_fetcher.h"

#include "base/command_line.h"
#include "components/autofill_assistant/browser/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Eq;

TEST(ServerUrlFetcherTest, GetDefaultServerUrl) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kAutofillAssistantUrl);
  EXPECT_THAT(ServerUrlFetcher::GetDefaultServerUrl(),
              Eq(GURL("https://automate-pa.googleapis.com")));
}

TEST(ServerUrlFetcherTest, GetDefaultServerUrlCommandLineOverride) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantUrl, "https://www.example.com");
  EXPECT_THAT(ServerUrlFetcher::GetDefaultServerUrl(),
              Eq(GURL("https://www.example.com")));
}

TEST(ServerUrlFetcherTest, GetScriptsEndpoint) {
  EXPECT_THAT(ServerUrlFetcher(GURL("https://www.example.com"))
                  .GetSupportsScriptEndpoint(),
              Eq(GURL("https://www.example.com/v1/supportsSite2")));
}

TEST(ServerUrlFetcherTest, GetActionsEndpoint) {
  EXPECT_THAT(ServerUrlFetcher(GURL("https://www.example.com"))
                  .GetNextActionsEndpoint(),
              Eq(GURL("https://www.example.com/v1/actions2")));
}

TEST(ServerUrlFetcherTest, GetTriggerScriptsEndpoint) {
  EXPECT_THAT(ServerUrlFetcher(GURL("https://www.example.com"))
                  .GetTriggerScriptsEndpoint(),
              Eq(GURL("https://www.example.com/v1/triggers")));
}

}  // namespace
}  // namespace autofill_assistant
