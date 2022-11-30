// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_info.h"

#include <memory>
#include <string>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath GetKernel32DllFilePath() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string sysroot;
  EXPECT_TRUE(env->GetVar("SYSTEMROOT", &sysroot));

  base::FilePath path =
      base::FilePath::FromUTF8Unsafe(sysroot).Append(L"system32\\kernel32.dll");

  return path;
}

}  // namespace

TEST(ModuleInfoTest, InspectModule) {
  ModuleInspectionResult inspection_result =
      InspectModule(GetKernel32DllFilePath());

  EXPECT_EQ(u"c:\\windows\\system32\\", inspection_result.location);
  EXPECT_EQ(u"kernel32.dll", inspection_result.basename);
  EXPECT_EQ(u"Microsoft\xAE Windows\xAE Operating System",
            inspection_result.product_name);
  EXPECT_EQ(u"Windows NT BASE API Client DLL", inspection_result.description);
  EXPECT_FALSE(inspection_result.version.empty());
  EXPECT_EQ(inspection_result.certificate_info.type,
            CertificateInfo::Type::CERTIFICATE_IN_CATALOG);
  EXPECT_FALSE(inspection_result.certificate_info.path.empty());
  EXPECT_EQ(u"Microsoft Windows", inspection_result.certificate_info.subject);
}

TEST(ModuleInfoTest, GenerateCodeId) {
  static const char kExpected[] = "00000BADf00d";
  ModuleInfoKey module_key = {base::FilePath(), 0xf00d, 0xbad};
  EXPECT_STREQ(kExpected, GenerateCodeId(module_key).c_str());
}

TEST(ModuleInfoTest, NormalizeInspectionResult) {
  ModuleInspectionResult test_case;
  test_case.location = u"%variable%\\PATH\\TO\\file.txt";
  test_case.version = u"23, 32, 43, 55 win7_rtm.123456-1234";

  ModuleInspectionResult expected;
  expected.location = u"%variable%\\path\\to\\";
  expected.basename = u"file.txt";
  expected.version = u"23.32.43.55";

  internal::NormalizeInspectionResult(&test_case);

  EXPECT_EQ(test_case.location, expected.location);
  EXPECT_EQ(test_case.basename, expected.basename);
  EXPECT_EQ(test_case.version, expected.version);
}
