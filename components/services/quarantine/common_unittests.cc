// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/common.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(QuarantineCommonTest, SanitizeUrlForQuarantine) {
  struct {
    const char* input;
    const char* expected_output;
  } kTestCases[] = {
      // Credentials stripped from http URL.
      {"http://foo:bar@s.example/x/y/z", "http://s.example/x/y/z"},

      // Preserve query and fragment of http{s} URL.
      {"http://a.example/x/y?q=f&r=g#blah",
       "http://a.example/x/y?q=f&r=g#blah"},
      {"https://a.example/x/y?q=f#h", "https://a.example/x/y?q=f#h"},

      // Ditto for ws{s} URL.
      {"ws://a.example/x/y?q=f", "ws://a.example/x/y?q=f"},
      {"wss://a.example/x/y?q=f", "wss://a.example/x/y?q=f"},

      // Credentials stripped from wss URL.
      {"wss://foo:bar@a.example/x/y?q=f", "wss://a.example/x/y?q=f"},

      // blob URLs get reduced to origin.
      {"blob:https://b.example/x/y/z", "https://b.example/"},
      {"blob:https://foo:bar@b.example/x/y/z?q", "https://b.example/"},

      // filesystem URLs get reduced to origin.
      {"filesystem:https://example.com/temporary/m", "https://example.com/"},
      {"filesystem:https://foo:bar@example.com/temporary/m",
       "https://example.com/"},

      // Unknown scheme is passed through as-is.
      {"some-random-scheme:randomdata", "some-random-scheme:randomdata"},

      // data URL is dropped.
      {"data:text/plain,hello%20world!", ""},

      // Invalid URL is dropped.
      {"1|\\|\\/4L||>", ""},
  };

  for (const auto test_case : kTestCases) {
    GURL input_url{test_case.input};
    GURL output = quarantine::SanitizeUrlForQuarantine(input_url);
    EXPECT_EQ(test_case.expected_output, output)
        << "Input : " << test_case.input;
  }
}
