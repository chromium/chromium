// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/persisted_data.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/version.h"
#include "chrome/updater/registration_data.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(PersistedDataTest, Simple) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(pref.get());

  EXPECT_FALSE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_TRUE(metadata->GetFingerprint("someappid").empty());
  EXPECT_TRUE(metadata->GetAppIds().empty());

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());

  metadata->SetFingerprint("someappid", "fp1");
  EXPECT_STREQ("fp1", metadata->GetFingerprint("someappid").c_str());

  // Store some more apps in prefs, in addition to "someappid". Expect only
  // the app ids for apps with valid versions to be returned.
  metadata->SetProductVersion("appid1", base::Version("2.0"));
  metadata->SetFingerprint("appid2-nopv", "somefp");
  EXPECT_FALSE(metadata->GetProductVersion("appid2-nopv").IsValid());
  const auto app_ids = metadata->GetAppIds();
  EXPECT_EQ(2u, app_ids.size());
  EXPECT_TRUE(base::Contains(app_ids, "someappid"));
  EXPECT_TRUE(base::Contains(app_ids, "appid1"));
  EXPECT_FALSE(base::Contains(app_ids, "appid2-nopv"));  // No valid pv.
}

TEST(PersistedDataTest, RegistrationRequest) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(pref.get());

  RegistrationRequest data;
  data.app_id = "someappid";
  data.brand_code = "somebrand";
  data.tag = "arandom-tag=likethis";
  data.version = base::Version("1.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  metadata->RegisterApp(data);
  EXPECT_TRUE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath("someappid").value());
  EXPECT_STREQ("arandom-tag=likethis", metadata->GetTag("someappid").c_str());
  EXPECT_STREQ("somebrand", metadata->GetBrandCode("someappid").c_str());
}

TEST(PersistedDataTest, SharedPref) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(pref.get());

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());

  // Now, create a new PersistedData reading from the same path, verify
  // that it loads the value.
  metadata = base::MakeRefCounted<PersistedData>(pref.get());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
}

TEST(PersistedDataTest, RemoveAppId) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(pref.get());

  RegistrationRequest data;
  data.app_id = "someappid";
  data.brand_code = "somebrand";
  data.tag = "arandom-tag=likethis";
  data.version = base::Version("1.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  metadata->RegisterApp(data);

  data.app_id = "someappid2";
  data.brand_code = "somebrand";
  data.tag = "arandom-tag=likethis";
  data.version = base::Version("2.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  metadata->RegisterApp(data);
  EXPECT_EQ(size_t{2}, metadata->GetAppIds().size());

  metadata->RemoveApp("someappid");
  EXPECT_EQ(size_t{1}, metadata->GetAppIds().size());

  metadata->RemoveApp("someappid2");
  EXPECT_TRUE(metadata->GetAppIds().empty());
}

}  // namespace updater
