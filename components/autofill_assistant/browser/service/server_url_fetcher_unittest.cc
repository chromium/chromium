// Copyright 2020 The Chromium Authors
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

TEST(ServerUrlFetcherTest, IsProdEndpoint) {
  ServerUrlFetcher points_to_prod = {
      GURL("https://automate-pa.googleapis.com")};
  EXPECT_TRUE(points_to_prod.IsProdEndpoint());

  ServerUrlFetcher points_to_test = {GURL("https://example.com")};
  EXPECT_FALSE(points_to_test.IsProdEndpoint());
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

TEST(ServerUrlFetcherTest, GetCapabilitiesByHashEndpoint) {
  EXPECT_THAT(ServerUrlFetcher(GURL("https://www.example.com"))
                  .GetCapabilitiesByHashEndpoint(),
              Eq(GURL("https://www.example.com/v1/capabilitiesByHashPrefix2")));
}

TEST(ServerUrlFetcherTest, GetUserDataEndpoint) {
  EXPECT_THAT(
      ServerUrlFetcher(GURL("https://www.example.com")).GetUserDataEndpoint(),
      Eq(GURL("https://www.example.com/v1/userData")));
}

TEST(ServerUrlFetcherTest, GetReportProgressEndpoint) {
  EXPECT_THAT(ServerUrlFetcher(GURL("https://www.example.com"))
                  .GetReportProgressEndpoint(),
              Eq(GURL("https://www.example.com/v1/reportProgress")));
}

}  // namespace
}  // namespace autofill_assistant
