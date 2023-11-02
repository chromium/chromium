// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/app_categorizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char* kChatAppURLs[] = {
  "https://hangouts.google.com/hangouts/foo",
  "https://hAnGoUtS.gOoGlE.com/HaNgOuTs/foo",
  "https://meet.google.com/hangouts/foo",
  "https://talkgadget.google.com/hangouts/foo",
  "https://staging.talkgadget.google.com/hangouts/foo",
  "https://plus.google.com/hangouts/foo",
  "https://plus.sandbox.google.com/hangouts/foo"
};

const char* kChatManifestFSs[] = {
  "filesystem:https://hangouts.google.com/foo",
  "filesystem:https://hAnGoUtS.gOoGlE.com/foo",
  "filesystem:https://meet.google.com/foo",
  "filesystem:https://talkgadget.google.com/foo",
  "filesystem:https://staging.talkgadget.google.com/foo",
  "filesystem:https://plus.google.com/foo",
  "filesystem:https://plus.sandbox.google.com/foo"
};

const char* kBadChatAppURLs[] = {
  "http://talkgadget.google.com/hangouts/foo",  // not https
  "https://talkgadget.evil.com/hangouts/foo"    // domain not whitelisted
};

}  // namespace

TEST(AppCategorizerTest, IsHangoutsUrl) {
  for (size_t i = 0; i < std::size(kChatAppURLs); ++i) {
    EXPECT_TRUE(AppCategorizer::IsHangoutsUrl(GURL(kChatAppURLs[i])));
  }

  for (size_t i = 0; i < std::size(kBadChatAppURLs); ++i) {
    EXPECT_FALSE(AppCategorizer::IsHangoutsUrl(GURL(kBadChatAppURLs[i])));
  }
}

TEST(AppCategorizerTest, IsWhitelistedApp) {
  // Hangouts app
  {
    EXPECT_EQ(std::size(kChatAppURLs), std::size(kChatManifestFSs));
    for (size_t i = 0; i < std::size(kChatAppURLs); ++i) {
      EXPECT_TRUE(AppCategorizer::IsWhitelistedApp(
          GURL(kChatManifestFSs[i]), GURL(kChatAppURLs[i])));
    }
    for (size_t i = 0; i < std::size(kBadChatAppURLs); ++i) {
      EXPECT_FALSE(AppCategorizer::IsWhitelistedApp(
          GURL("filesystem:https://irrelevant.com/"),
          GURL(kBadChatAppURLs[i])));
    }

    // Manifest URL not filesystem
    EXPECT_FALSE(AppCategorizer::IsWhitelistedApp(
        GURL("https://hangouts.google.com/foo"),
        GURL("https://hangouts.google.com/hangouts/foo")));

    // Manifest URL not https
    EXPECT_FALSE(AppCategorizer::IsWhitelistedApp(
        GURL("filesystem:http://hangouts.google.com/foo"),
        GURL("https://hangouts.google.com/hangouts/foo")));

    // Manifest URL hostname does not match that of the app URL
    EXPECT_FALSE(AppCategorizer::IsWhitelistedApp(
        GURL("filesystem:https://meet.google.com/foo"),
        GURL("https://hangouts.google.com/hangouts/foo")));
  }
}
