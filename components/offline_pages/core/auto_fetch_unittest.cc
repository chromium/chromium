// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/auto_fetch.h"

#include "components/offline_pages/core/client_namespace_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace auto_fetch {
namespace {

TEST(AutoFetch, MakeClientId) {
  EXPECT_EQ(ClientId(kAutoAsyncNamespace, "A123"),
            auto_fetch::MakeClientId(auto_fetch::ClientIdMetadata(123)));
}

TEST(AutoFetch, ExtractMetadataSuccess) {
  std::optional<auto_fetch::ClientIdMetadata> metadata =
      auto_fetch::ExtractMetadata(
          auto_fetch::MakeClientId(auto_fetch::ClientIdMetadata(123)));
  ASSERT_TRUE(metadata);
  EXPECT_EQ(123, metadata.value().android_tab_id);
}

TEST(AutoFetch, ExtractMetadataFail) {
  EXPECT_FALSE(
      auto_fetch::ExtractMetadata(ClientId(kAutoAsyncNamespace, "123")));
  EXPECT_FALSE(auto_fetch::ExtractMetadata(ClientId(kAutoAsyncNamespace, "")));
  EXPECT_FALSE(auto_fetch::ExtractMetadata(ClientId("other", "A123")));
}

}  // namespace
}  // namespace auto_fetch
}  // namespace offline_pages
