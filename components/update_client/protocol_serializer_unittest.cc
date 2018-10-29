// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_util.h"
#include "base/version.h"
#include "components/update_client/protocol_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace update_client {

TEST(BuildProtocolRequest, BuildUpdateCheckExtraRequestHeaders) {
  auto headers = BuildUpdateCheckExtraRequestHeaders(
      "fake_prodid", base::Version("30.0"), {}, true);
  EXPECT_EQ("fake_prodid-30.0", headers["X-Goog-Update-Updater"]);
  EXPECT_EQ("fg", headers["X-Goog-Update-Interactivity"]);
  EXPECT_TRUE(headers["X-Goog-Update-AppId"].empty());

  headers = BuildUpdateCheckExtraRequestHeaders(
      "fake_prodid", base::Version("30.0"), {}, false);
  EXPECT_EQ("fake_prodid-30.0", headers["X-Goog-Update-Updater"]);
  EXPECT_EQ("bg", headers["X-Goog-Update-Interactivity"]);
  EXPECT_TRUE(headers["X-Goog-Update-AppId"].empty());

  headers = BuildUpdateCheckExtraRequestHeaders(
      "fake_prodid", base::Version("30.0"),
      {"jebgalgnebhfojomionfpkfelancnnkf"}, true);
  EXPECT_EQ("fake_prodid-30.0", headers["X-Goog-Update-Updater"]);
  EXPECT_EQ("fg", headers["X-Goog-Update-Interactivity"]);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", headers["X-Goog-Update-AppId"]);

  headers = BuildUpdateCheckExtraRequestHeaders(
      "fake_prodid", base::Version("30.0"),
      {"jebgalgnebhfojomionfpkfelancnnkf", "ihfokbkgjpifbbojhneepfflplebdkc"},
      true);
  EXPECT_EQ("fake_prodid-30.0", headers["X-Goog-Update-Updater"]);
  EXPECT_EQ("fg", headers["X-Goog-Update-Interactivity"]);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf,ihfokbkgjpifbbojhneepfflplebdkc",
            headers["X-Goog-Update-AppId"]);

  // Test that only 30 extension ids are joined in the headers.
  headers = BuildUpdateCheckExtraRequestHeaders(
      "fake_prodid", base::Version("30.0"),
      std::vector<std::string>(40, "jebgalgnebhfojomionfpkfelancnnkf"), true);
  EXPECT_EQ(base::JoinString(std::vector<std::string>(
                                 30, "jebgalgnebhfojomionfpkfelancnnkf"),
                             ","),
            headers["X-Goog-Update-AppId"]);
}

}  // namespace update_client
