// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/test/test_meta_info.h"

#include "base/command_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {
namespace {

TEST(HeadlessTestMetaInfoTest, NoMetaInfo) {
  auto meta_info = TestMetaInfo::FromString(R"(
  // Copyright 2025 The Chromium Authors
  //
  // )");

  ASSERT_TRUE(meta_info.has_value());

  EXPECT_TRUE(meta_info.value().IsEmpty());
}

TEST(HeadlessTestMetaInfoTest, UnknownMetaInfo) {
  auto meta_info = TestMetaInfo::FromString(R"(
  // META: foobar
  // )");

  ASSERT_FALSE(meta_info.has_value());

  EXPECT_EQ(meta_info.error(), "Invalid meta info: foobar");
}

TEST(HeadlessTestMetaInfoTest, MissingMetaInfoContinuation) {
  auto meta_info = TestMetaInfo::FromString(R"(
  // META: foobar \
  //
  // )");

  ASSERT_FALSE(meta_info.has_value());

  EXPECT_EQ(meta_info.error(), "Invalid meta info: foobar \\");
}

TEST(HeadlessTestMetaInfoTest, NoLeadingMetaInfoWhiteSpace) {
  auto meta_info = TestMetaInfo::FromString(R"(
  // META:foobar
  // )");

  ASSERT_FALSE(meta_info.has_value());

  EXPECT_EQ(meta_info.error(), "Invalid meta info: foobar");
}

TEST(HeadlessTestMetaInfoTest, ExtraLeadingMetaInfoWhiteSpace) {
  auto meta_info = TestMetaInfo::FromString(R"(
  // META:    foobar
  //
  // )");

  ASSERT_FALSE(meta_info.has_value());

  EXPECT_EQ(meta_info.error(), "Invalid meta info: foobar");
}

TEST(HeadlessTestMetaInfoTest, TrailingMetaInfoWhiteSpace) {
  auto meta_info = TestMetaInfo::FromString("// META: foobar   ");

  ASSERT_FALSE(meta_info.has_value());

  EXPECT_EQ(meta_info.error(), "Invalid meta info: foobar");
}

TEST(HeadlessTestMetaInfoTest, TrailingMetaInfoWhiteSpaceAfterContinuation) {
  auto meta_info = TestMetaInfo::FromString(
      "// META: foobar \\  \n"
      "// META: and foobar \n");

  ASSERT_FALSE(meta_info.has_value());

  EXPECT_EQ(meta_info.error(), "Invalid meta info: foobar and foobar");
}

TEST(HeadlessTestMetaInfoTest, CommandLineSwitch) {
  auto meta_info = TestMetaInfo::FromString(R"(
  // Copyright 2025 The Chromium Authors
  // META: --window-size=800,600
  // META: --screen-info={1600x1200 \
  // META: workAreaLeft=10 workAreaRight=90 \
  // META: workAreaTop=20 workAreaBottom=80 \
  // META: label='primary screen'}
  //
  // META: --single-process
  // )");

  ASSERT_TRUE(meta_info.has_value());

  EXPECT_FALSE(meta_info.value().IsEmpty());

  base::CommandLine command_line(0, nullptr);
  meta_info.value().ProcessCommandLineSwitches(command_line);

  EXPECT_TRUE(command_line.HasSwitch("single-process"));
  EXPECT_TRUE(command_line.HasSwitch("window-size"));
  EXPECT_TRUE(command_line.HasSwitch("screen-info"));

  EXPECT_EQ(command_line.GetSwitchValueASCII("single-process"), "");
  EXPECT_EQ(command_line.GetSwitchValueASCII("window-size"), "800,600");
  EXPECT_EQ(command_line.GetSwitchValueASCII("screen-info"),
            "{1600x1200 workAreaLeft=10 workAreaRight=90 workAreaTop=20 "
            "workAreaBottom=80 label='primary screen'}");
}

TEST(HeadlessTestMetaInfoTest, CompositeCommandLineSwitch) {
  auto meta_info = TestMetaInfo::FromString(R"(
  // META: --js-flags=--expose-gc,--allow-natives-syntax
  // )");

  ASSERT_TRUE(meta_info.has_value());

  EXPECT_FALSE(meta_info.value().IsEmpty());

  base::CommandLine command_line(0, nullptr);
  meta_info.value().ProcessCommandLineSwitches(command_line);

  EXPECT_EQ(command_line.GetSwitchValueASCII("js-flags"),
            "--expose-gc,--allow-natives-syntax");
}

TEST(HeadlessTestMetaInfoTest, FeaturesCommandLineSwitches) {
  auto meta_info = TestMetaInfo::FromString(R"(
  // META: --enable-features=a,b,c
  // META: --disable-features=d,e,f
  // )");

  ASSERT_TRUE(meta_info.has_value());

  EXPECT_FALSE(meta_info.value().IsEmpty());

  base::CommandLine command_line(0, nullptr);
  auto scoped_feature_list =
      meta_info.value().ProcessCommandLineSwitches(command_line);
  EXPECT_NE(scoped_feature_list.get(), nullptr);

  std::string enabled_features;
  std::string disabled_features;
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  feature_list->GetFeatureOverrides(&enabled_features, &disabled_features);

  EXPECT_THAT(enabled_features, testing::HasSubstr("a,b,c"));
  EXPECT_THAT(disabled_features, testing::HasSubstr("d,e,f"));

  EXPECT_FALSE(command_line.HasSwitch("enable-features"));
  EXPECT_FALSE(command_line.HasSwitch("disable-features"));
}

TEST(HeadlessTestMetaInfoTest, ForkHeadlessModeExpectations) {
  ASSERT_FALSE(TestMetaInfo().fork_headless_mode_expectations());

  auto meta_info = TestMetaInfo::FromString(R"(
  // META: fork_headless_mode_expectations
  // )");

  ASSERT_TRUE(meta_info.has_value());

  EXPECT_FALSE(meta_info.value().IsEmpty());

  EXPECT_TRUE(meta_info.value().fork_headless_mode_expectations());
}

TEST(HeadlessTestMetaInfoTest, ForkHeadlessShellExpectations) {
  ASSERT_FALSE(TestMetaInfo().fork_headless_shell_expectations());

  auto meta_info = TestMetaInfo::FromString(R"(
  // META: fork_headless_shell_expectations
  // )");

  ASSERT_TRUE(meta_info.has_value());

  EXPECT_FALSE(meta_info.value().IsEmpty());

  EXPECT_TRUE(meta_info.value().fork_headless_shell_expectations());
}

}  // namespace
}  // namespace headless
