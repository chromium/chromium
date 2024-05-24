// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/file_info.h"

#include <sstream>
#include <string>

#include "base/logging.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace ash::file_manager {
namespace {

class FileInfoTest : public testing::Test {
 public:
  void SetUp() override {
    scheme_registry_ = std::make_unique<url::ScopedSchemeRegistryForTests>();
    url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
  }

 private:
  // Allows registering the "chrome://" scheme, without depending on //content.
  std::unique_ptr<url::ScopedSchemeRegistryForTests> scheme_registry_;
};

TEST_F(FileInfoTest, RemoteId) {
  GURL url("filesystem:chrome://file-manager/external/Downloads-user123/a.txt");
  FileInfo file_info(url, 100, base::Time::Now());

  ASSERT_FALSE(file_info.remote_id.has_value());
  const std::string remote_id("46bd8697");
  file_info.remote_id = remote_id;
  EXPECT_TRUE(file_info.remote_id.has_value());
  EXPECT_EQ(file_info.remote_id.value(), remote_id);
}

TEST_F(FileInfoTest, ToString) {
  GURL url("filesystem:chrome://file-manager/external/Downloads-u123/a.txt");
  base::Time last_modified;
  ASSERT_TRUE(
      base::Time::FromUTCString("19 Jan 2023 12:00 UTC", &last_modified));

  FileInfo file_info(url, 100, last_modified);

  {
    std::stringstream ss;
    ss << file_info;
    EXPECT_EQ(ss.str(),
              "FileInfo(file_url=filesystem:chrome://file-manager/"
              "external/Downloads-u123/a.txt, size=100, last_modified"
              "=2023-01-19 12:00:00.000000 UTC, remote_id=null)");
  }

  file_info.remote_id = "remote-id";
  {
    std::stringstream ss;
    ss << file_info;
    EXPECT_EQ(ss.str(),
              "FileInfo(file_url=filesystem:chrome://file-manager/"
              "external/Downloads-u123/a.txt, size=100, last_modified"
              "=2023-01-19 12:00:00.000000 UTC, remote_id=remote-id)");
  }
}

TEST_F(FileInfoTest, Equality) {
  GURL url_a("filesystem:chrome://file-manager/external/Downloads-u123/a.txt");
  GURL url_b("filesystem:chrome://file-manager/external/Downloads-u123/b.txt");
  base::Time last_modified_a;
  ASSERT_TRUE(
      base::Time::FromUTCString("19 Jan 2023 12:00 UTC", &last_modified_a));
  base::Time last_modified_b;
  ASSERT_TRUE(
      base::Time::FromUTCString("19 Feb 2023 12:00 UTC", &last_modified_b));
  int64_t size_a = 100;
  int64_t size_b = 200;

  const std::string remote_id_a("remote_id_a");
  const std::string remote_id_b("remote_id_b");

  EXPECT_EQ(FileInfo(url_a, size_a, last_modified_a),
            FileInfo(url_a, size_a, last_modified_a));
  EXPECT_NE(FileInfo(url_a, size_a, last_modified_a),
            FileInfo(url_b, size_a, last_modified_a));
  EXPECT_NE(FileInfo(url_a, size_a, last_modified_a),
            FileInfo(url_a, size_b, last_modified_a));
  EXPECT_NE(FileInfo(url_a, size_a, last_modified_a),
            FileInfo(url_a, size_a, last_modified_b));

  FileInfo info_a(url_a, size_a, last_modified_a);
  FileInfo info_b(url_a, size_a, last_modified_a);
  info_a.remote_id = remote_id_a;
  EXPECT_NE(info_a, info_b);
  info_a.remote_id.reset();
  info_b.remote_id = remote_id_b;
  EXPECT_NE(info_a, info_b);
  info_a.remote_id = remote_id_a;
  EXPECT_NE(info_a, info_b);
  info_b.remote_id = remote_id_a;
  EXPECT_EQ(info_a, info_b);
}

TEST_F(FileInfoTest, UrlValidity) {
  GURL url("filesystem:chrome://file-manager/external/Downloads-u123/a.txt");
  EXPECT_TRUE(url.is_valid());
}

}  // namespace
}  // namespace ash::file_manager
