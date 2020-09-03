// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants_unittest.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/win/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {

namespace {

void ClearUserDefaults() {
  base::win::RegKey key(HKEY_CURRENT_USER, L"", KEY_SET_VALUE);
  key.DeleteKey(UPDATE_DEV_KEY);
}

}  // namespace

void DevOverrideTest::SetUp() {
  ClearUserDefaults();
}

void DevOverrideTest::TearDown() {
  ClearUserDefaults();
}

TEST_F(DevOverrideTest, TestDevOverrides) {
  std::unique_ptr<ExternalConstants> consts = CreateExternalConstants();

  base::win::RegKey key;
  const base::char16 val[] = L"http://localhost:8080";
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, UPDATE_DEV_KEY, KEY_WRITE),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(base::UTF8ToUTF16(kDevOverrideKeyUrl).c_str(), val),
            ERROR_SUCCESS);
  DWORD use_cup = 0;
  ASSERT_EQ(
      key.WriteValue(base::UTF8ToUTF16(kDevOverrideKeyUseCUP).c_str(), use_cup),
      ERROR_SUCCESS);

  ASSERT_FALSE(consts->UseCUP());
  std::vector<GURL> urls = consts->UpdateURL();
  ASSERT_EQ(urls.size(), 1u);
  ASSERT_EQ(urls[0], GURL("http://localhost:8080"));
  ASSERT_TRUE(urls[0].is_valid());
}

}  // namespace updater
