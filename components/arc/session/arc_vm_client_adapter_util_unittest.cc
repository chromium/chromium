// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter_util.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chromeos/constants/chromeos_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kCrosConfigPropertiesPath[] = "/arc/build-properties";

class ArcVmClientAdapterUtilTest : public testing::Test {
 public:
  ArcVmClientAdapterUtilTest() = default;
  ~ArcVmClientAdapterUtilTest() override = default;
  ArcVmClientAdapterUtilTest(const ArcVmClientAdapterUtilTest&) = delete;
  ArcVmClientAdapterUtilTest& operator=(const ArcVmClientAdapterUtilTest&) =
      delete;

  void SetUp() override { ASSERT_TRUE(dir_.CreateUniqueTempDir()); }

 protected:
  const base::FilePath& GetTempDir() const { return dir_.GetPath(); }

  CrosConfig* config() {
    if (!config_)
      config_ = std::make_unique<CrosConfig>();
    return config_.get();
  }

 private:
  std::unique_ptr<CrosConfig> config_;
  base::ScopedTempDir dir_;
};

// Tests that the GetString method works as intended.
TEST_F(ArcVmClientAdapterUtilTest, CrosConfig_GetString) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "{\"k1\":\"v1\",\"k2\":\"v2\",\"k3\":3}");
  std::string str;
  EXPECT_TRUE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
  EXPECT_EQ("v1", str);
  EXPECT_TRUE(config()->GetString(kCrosConfigPropertiesPath, "k2", &str));
  EXPECT_EQ("v2", str);
  // The value is not a string.
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k3", &str));
  // The property path is invalid.
  EXPECT_FALSE(config()->GetString("/unknown/path", "k1", &str));
}

// Tests that CrosConfig can handle the case where the command line is not
// passed.
TEST_F(ArcVmClientAdapterUtilTest, CrosConfig_NoCommandline) {
  std::string str;
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
}

// Tests that CrosConfig can handle an empty command line.
TEST_F(ArcVmClientAdapterUtilTest, CrosConfig_EmptyCommandline) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties, "");
  std::string str;
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
}

// Tests that CrosConfig can handle JSON whose top-level is not a dict.
TEST_F(ArcVmClientAdapterUtilTest, CrosConfig_NoDict) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "[\"k1\"]");
  std::string str;
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
}

// Tests that CrosConfig can handle an invalid JSON.
TEST_F(ArcVmClientAdapterUtilTest, CrosConfig_InvalidJson) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "{\"k1\":}");  // parse error: no value
  std::string str;
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
}

// Tests that ExpandPropertyFile works as intended when no property expantion
// is needed.
TEST_F(ArcVmClientAdapterUtilTest, ExpandPropertyFile_NoExpansion) {
  constexpr const char kValidProp[] = "ro.foo=bar\nro.baz=boo";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));

  const base::FilePath dest = GetTempDir().Append("new.prop");
  EXPECT_TRUE(ExpandPropertyFile(path, dest, config()));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest, &content));
  // Note: ExpandPropertyFile() adds a trailing LF.
  EXPECT_EQ(std::string(kValidProp) + "\n", content);
}

// Tests that ExpandPropertyFile works as intended when property expantion
// is needed.
TEST_F(ArcVmClientAdapterUtilTest, ExpandPropertyFile_Expansion) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "{\"k1\":\"v1\",\"k2\":\"v2\"}");
  constexpr const char kValidProp[] = "ro.foo={k1}\nro.baz={k2}";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));

  const base::FilePath dest = GetTempDir().Append("new.prop");
  EXPECT_TRUE(ExpandPropertyFile(path, dest, config()));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest, &content));
  // Note: ExpandPropertyFile() adds a trailing LF.
  EXPECT_EQ("ro.foo=v1\nro.baz=v2\n", content);
}

// Tests that ExpandPropertyFile works as intended when nested property
// expantion is needed.
TEST_F(ArcVmClientAdapterUtilTest, ExpandPropertyFile_NestedExpansion) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "{\"k1\":\"{k2}\",\"k2\":\"v2\"}");
  constexpr const char kValidProp[] = "ro.foo={k1}\nro.baz={k2}";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));

  const base::FilePath dest = GetTempDir().Append("new.prop");
  EXPECT_TRUE(ExpandPropertyFile(path, dest, config()));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest, &content));
  // Note: ExpandPropertyFile() adds a trailing LF.
  EXPECT_EQ("ro.foo=v2\nro.baz=v2\n", content);
}

// Test that ExpandPropertyFile handles the case where a property is not found.
TEST_F(ArcVmClientAdapterUtilTest, ExpandPropertyFile_CannotExpand) {
  constexpr const char kValidProp[] =
      "ro.foo={nonexistent-property}\nro.baz=boo\n";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));
  const base::FilePath dest = GetTempDir().Append("new.prop");
  EXPECT_FALSE(ExpandPropertyFile(path, dest, config()));
}

// Test that ExpandPropertyFile handles the case where the input file is not
// found.
TEST_F(ArcVmClientAdapterUtilTest, ExpandPropertyFile_NoSourceFile) {
  EXPECT_FALSE(ExpandPropertyFile(base::FilePath("/nonexistent"),
                                  base::FilePath("/nonexistent2"), config()));
}

// Test that ExpandPropertyFile handles the case where the output file cannot
// be written.
TEST_F(ArcVmClientAdapterUtilTest, ExpandPropertyFile_CannotWrite) {
  constexpr const char kValidProp[] = "ro.foo=bar\nro.baz=boo\n";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));
  EXPECT_FALSE(
      ExpandPropertyFile(path, base::FilePath("/nonexistent2"), config()));
}

}  // namespace
}  // namespace arc
