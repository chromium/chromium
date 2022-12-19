// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

class NameManifestTest : public ChromeManifestTest {
 protected:
  ManifestData GetManifestData(const char* name, int manifest_version) {
    static constexpr char kManifestStub[] =
        R"({
            "name": %s,
            "version": "0.1",
            "manifest_version": %d,
        })";
    base::Value manifest_value = base::test::ParseJson(
        base::StringPrintf(kManifestStub, name, manifest_version));
    CHECK(manifest_value.is_dict());
    return ManifestData(std::move(manifest_value).TakeDict());
  }
};

TEST_F(NameManifestTest, NonEmptyName) {
  static constexpr struct {
    const char* title;
    const char* name;
    int manifest_version;
  } test_cases[] = {{"Succeed when name is non-empty.", R"("Ok")", 3},
                    {"Succeed when name is non-empty.", R"("Ok")", 2}};
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    LoadAndExpectSuccess(
        GetManifestData(test_case.name, test_case.manifest_version));
  }
}

TEST_F(NameManifestTest, EmptyName) {
  static constexpr struct {
    const char* title;
    const char* name;
    int manifest_version;
  } test_cases[] = {{"Error when name is empty.", R"("")", 3},
                    {"Error when name is empty.", R"("")", 2}};
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    LoadAndExpectError(
        GetManifestData(test_case.name, test_case.manifest_version),
        "Required value 'name' is missing or invalid.");
  }
}
