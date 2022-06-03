// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/cpu_temperature_reader.h"

#include <algorithm>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace system {

class CPUTemperatureReaderTest : public ::testing::Test {
 public:
  CPUTemperatureReaderTest() {
    CHECK(dir_.CreateUniqueTempDir());
    hwmon_path_ = dir_.GetPath();
    reader_.set_hwmon_dir_for_test(hwmon_path_);
  }

  ~CPUTemperatureReaderTest() override = default;

 protected:
  using CPUTemperatureInfo = CPUTemperatureReader::CPUTemperatureInfo;

  // Creates a subdirectory in |hwmon_path_| with name |name|. Returns the full
  // path of the new subdirectory.
  base::FilePath CreateHwmonSubdir(const std::string& name) {
    base::FilePath subdir_path = hwmon_path_.Append(name);
    CHECK(base::CreateDirectory(subdir_path));
    return subdir_path;
  }

  // Creates a file at |path| containing data |contents|.
  void CreateFileWithContents(const base::FilePath& path,
                              const std::string& contents) {
    CHECK_EQ(WriteFile(path, contents.data(), contents.size()),
             static_cast<int>(contents.size()));
  }

  // Creates a temporary dir to act as the hwmon directory passed to |reader_|.
  base::ScopedTempDir dir_;

  // Path of the temporary dir created by |dir_|.
  base::FilePath hwmon_path_;

  // Instance of the class under test
  CPUTemperatureReader reader_;
};

TEST_F(CPUTemperatureReaderTest, EmptyDir) {
  base::FilePath subdir_empty = CreateHwmonSubdir("hwmon0");
  base::FilePath subdir_not_temp = CreateHwmonSubdir("hwmon1");
  CreateFileWithContents(subdir_not_temp.Append("not_cpu_temp"), "garbage");

  EXPECT_EQ(0U, reader_.GetCPUTemperatures().size());
}

TEST_F(CPUTemperatureReaderTest, SingleDir) {
  base::FilePath subdir = CreateHwmonSubdir("hwmon0");
  CreateFileWithContents(subdir.Append("temp1_input"), "10000\n");

  std::vector<CPUTemperatureInfo> cpu_temp_readings =
      reader_.GetCPUTemperatures();

  ASSERT_EQ(1U, cpu_temp_readings.size());
  EXPECT_EQ(10.0f, cpu_temp_readings[0].temp_celsius);
  EXPECT_EQ(subdir.Append("temp1_label").value(), cpu_temp_readings[0].label);
}

TEST_F(CPUTemperatureReaderTest, SingleDirWithLabel) {
  base::FilePath subdir = CreateHwmonSubdir("hwmon0");
  CreateFileWithContents(subdir.Append("temp2_input"), "20000\n");
  CreateFileWithContents(subdir.Append("temp2_label"), "t2\n");

  std::vector<CPUTemperatureInfo> cpu_temp_readings =
      reader_.GetCPUTemperatures();

  ASSERT_EQ(1U, cpu_temp_readings.size());
  EXPECT_EQ(20.0f, cpu_temp_readings[0].temp_celsius);
  EXPECT_EQ("t2", cpu_temp_readings[0].label);
}

TEST_F(CPUTemperatureReaderTest, SingleDirWithName) {
  base::FilePath subdir = CreateHwmonSubdir("hwmon0");
  CreateFileWithContents(subdir.Append("temp3_input"), "30000\n");
  CreateFileWithContents(subdir.Append("temp3_label"), "\n");
  CreateFileWithContents(subdir.Append("name"), "t3\n");

  std::vector<CPUTemperatureInfo> cpu_temp_readings =
      reader_.GetCPUTemperatures();

  ASSERT_EQ(1U, cpu_temp_readings.size());
  EXPECT_EQ(30.0f, cpu_temp_readings[0].temp_celsius);
  EXPECT_EQ("t3", cpu_temp_readings[0].label);
}

TEST_F(CPUTemperatureReaderTest, SingleDirWithDeviceSubdir) {
  base::FilePath subdir = CreateHwmonSubdir("hwmon0");
  CreateFileWithContents(subdir.Append("temp1_input"), "10000\n");
  base::FilePath device_subdir = subdir.Append("device");
  base::CreateDirectory(device_subdir);
  CreateFileWithContents(device_subdir.Append("temp1_input"), "20000\n");

  std::vector<CPUTemperatureInfo> cpu_temp_readings =
      reader_.GetCPUTemperatures();

  ASSERT_EQ(2U, cpu_temp_readings.size());
  EXPECT_EQ(10.0f, cpu_temp_readings[0].temp_celsius);
  EXPECT_EQ(subdir.Append("temp1_label").value(), cpu_temp_readings[0].label);
  EXPECT_EQ(20.0f, cpu_temp_readings[1].temp_celsius);
  EXPECT_EQ(device_subdir.Append("temp1_label").value(),
            cpu_temp_readings[1].label);
}

TEST_F(CPUTemperatureReaderTest, MultipleDirs) {
  base::FilePath subdir0 = CreateHwmonSubdir("hwmon0");
  CreateFileWithContents(subdir0.Append("temp1_input"), "10000\n");

  base::FilePath subdir1 = CreateHwmonSubdir("hwmon1");
  CreateFileWithContents(subdir1.Append("temp2_input"), "20000\n");
  CreateFileWithContents(subdir1.Append("temp2_label"), "t2\n");

  // This should not result in a CPU temperature reading.
  base::FilePath subdir2 = CreateHwmonSubdir("hwmon2");
  CreateFileWithContents(subdir2.Append("not_cpu_temp"), "garbage");

  base::FilePath subdir3 = CreateHwmonSubdir("hwmon3");
  CreateFileWithContents(subdir3.Append("temp3_input"), "30000\n");
  CreateFileWithContents(subdir3.Append("temp3_label"), "t3\n");

  std::vector<CPUTemperatureInfo> cpu_temp_readings =
      reader_.GetCPUTemperatures();

  // The order in which these directories were read is not guaranteed. Sort them
  // first.
  std::sort(cpu_temp_readings.begin(), cpu_temp_readings.end());

  ASSERT_EQ(3U, cpu_temp_readings.size());
  EXPECT_EQ(10.0f, cpu_temp_readings[0].temp_celsius);
  EXPECT_EQ(subdir0.Append("temp1_label").value(), cpu_temp_readings[0].label);
  EXPECT_EQ(20.0f, cpu_temp_readings[1].temp_celsius);
  EXPECT_EQ("t2", cpu_temp_readings[1].label);
  EXPECT_EQ(30.0f, cpu_temp_readings[2].temp_celsius);
  EXPECT_EQ("t3", cpu_temp_readings[2].label);
}

}  // namespace system
}  // namespace chromeos
