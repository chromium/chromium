// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/config_reader.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fuchsia_component_support {

TEST(ConfigReaderTest, NoConfigData) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  auto config = LoadConfigFromDirForTest(temp_dir.GetPath());
  EXPECT_FALSE(config.has_value());
}

TEST(ConfigReaderTest, SingleConfigJson) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("config.json"),
                              "{ \"name\": \"value\" }"));

  auto config = LoadConfigFromDirForTest(temp_dir.GetPath());
  ASSERT_TRUE(config.has_value());

  EXPECT_EQ(config->size(), 1u);
  const std::string* value = config->FindString("name");
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "value");
}

TEST(ConfigReaderTest, MultipleConfigJson) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("foo.json"),
                              "{ \"name1\": \"value?\" }"));
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("bar.json"),
                              "{ \"name2\": \"value!\" }"));
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("wibble.json"),
                              "{ \"name3\": \"value...\" }"));

  auto config = LoadConfigFromDirForTest(temp_dir.GetPath());
  ASSERT_TRUE(config.has_value());

  EXPECT_EQ(config->size(), 3u);

  std::string* value = config->FindString("name1");
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "value?");

  value = config->FindString("name2");
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "value!");

  value = config->FindString("name3");
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "value...");
}

TEST(ConfigReaderTest, OneOfTheseConfigsIsNotValidLikeTheOthers) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Provide some valid config JSONs.
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("foo.json"),
                              "{ \"name1\": \"value?\" }"));
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("wibble.json"),
                              "{ \"name2\": \"value...\" }"));

  // Provide an invalid one, which should cause a CHECK failure.
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("not_valid.json"),
                              "{ \"name3\"= }"));

  EXPECT_DEATH({ LoadConfigFromDirForTest(temp_dir.GetPath()); }, "");
}

TEST(ConfigReaderTest, MultipleClashingConfigJson) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("foo.json"),
                              "{ \"name\": \"value?\" }"));
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("bar.json"),
                              "{ \"name\": \"value!\" }"));
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append("wibble.json"),
                              "{ \"name\": \"value...\" }"));

  EXPECT_DEATH({ LoadConfigFromDirForTest(temp_dir.GetPath()); }, "");
}

}  // namespace fuchsia_component_support
