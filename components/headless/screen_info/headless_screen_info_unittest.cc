// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/screen_info/headless_screen_info.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {
namespace {

TEST(HeadlessScreenInfoTest, Basic) {
  EXPECT_EQ(HeadlessScreenInfo::FromString(" \t ").error(),
            "Invalid screen info:  \t ");

  EXPECT_EQ(HeadlessScreenInfo::FromString(" xyz ").error(),
            "Invalid screen info:  xyz ");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{").error(),
            "Invalid screen info: {");

  EXPECT_EQ(HeadlessScreenInfo::FromString("}").error(),
            "Invalid screen info: }");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{}").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(0, 0, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{  }").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(0, 0, 800, 600)}));
}

TEST(HeadlessScreenInfoTest, ScreenOrigin) {
  // Primary screen does not allow non zero origin, so test the secondary one.
  EXPECT_EQ(HeadlessScreenInfo::FromString("{}{100,200}").value()[1],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, 200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{}{ 100,200 }").value()[1],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, 200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{}{-100,200}").value()[1],
            HeadlessScreenInfo({.bounds = gfx::Rect(-100, 200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{}{100,-200}").value()[1],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, -200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{}{-100,-200}").value()[1],
            HeadlessScreenInfo({.bounds = gfx::Rect(-100, -200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{100,200}").error(),
            "Primary screen origin can only be at {0,0}");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100, 200}").error(),
            "Invalid screen info: 100, 200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz 100,200}").error(),
            "Invalid screen info: xyz 100,200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz100,200}").error(),
            "Invalid screen info: xyz100,200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100,200 xyz}").error(),
            "Invalid screen info: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100,200xyz}").error(),
            "Invalid screen info: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100+200 }").error(),
            "Invalid screen info: 100+200");
}

TEST(HeadlessScreenInfoTest, ScreenSize) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{100x200}").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, 200)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString(" { 100x200 } ").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, 200)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100x 200}").error(),
            "Invalid screen info: 100x 200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz 100x200}").error(),
            "Invalid screen info: xyz 100x200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz100x200}").error(),
            "Invalid screen info: xyz100x200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100x200 xyz}").error(),
            "Invalid screen info: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100x200xyz}").error(),
            "Invalid screen info: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100 200 }").error(),
            "Invalid screen info: 100 200");
}

TEST(HeadlessScreenInfoTest, ScreenParameters) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz =}").error(),
            "Invalid screen info: xyz =");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz = 42}").error(),
            "Invalid screen info: xyz = 42");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz=}").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz=42}").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz=}").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz=42}").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz= }").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz=42 }").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz= }").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz=42 }").error(),
            "Unknown screen info parameter: xyz");
}

TEST(HeadlessScreenInfoTest, ScreenColorDepth) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth=16 }").value()[0],
            HeadlessScreenInfo({.color_depth = 16}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth= 16 }").error(),
            "Invalid screen color depth: ");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth=0 }").error(),
            "Invalid screen color depth: 0");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth=x24 }").error(),
            "Invalid screen color depth: x24");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth=24x }").error(),
            "Invalid screen color depth: 24x");
}

TEST(HeadlessScreenInfoTest, ScreenDevicePixelRatio) {
  EXPECT_EQ(
      HeadlessScreenInfo::FromString("{ devicePixelRatio=0.5}").value()[0],
      HeadlessScreenInfo({.device_pixel_ratio = 0.5f}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ devicePixelRatio=4 }").value()[0],
            HeadlessScreenInfo({.device_pixel_ratio = 4.0f}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ devicePixelRatio=0.1 }").error(),
            "Invalid screen device pixel ratio: 0.1");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ devicePixelRatio= 1.0 }").error(),
            "Invalid screen device pixel ratio: ");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ devicePixelRatio=x1.0 }").error(),
            "Invalid screen device pixel ratio: x1.0");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{devicePixelRatio=1.0x }").error(),
            "Invalid screen device pixel ratio: 1.0x");
}

TEST(HeadlessScreenInfoTest, ScreenIsInternal) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=1 }").value()[0],
            HeadlessScreenInfo({.is_internal = true}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=true }").value()[0],
            HeadlessScreenInfo({.is_internal = true}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=0 }").value()[0],
            HeadlessScreenInfo({.is_internal = false}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=false }").value()[0],
            HeadlessScreenInfo({.is_internal = false}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal= }").error(),
            "Invalid screen is internal: ");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=xyz }").error(),
            "Invalid screen is internal: xyz");
}

TEST(HeadlessScreenInfoTest, ScreenLabel) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label=xyz}").value()[0],
            HeadlessScreenInfo({.label = "xyz"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label='xyz'}").value()[0],
            HeadlessScreenInfo({.label = "xyz"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label=''}").value()[0],
            HeadlessScreenInfo({.label = ""}));

  EXPECT_EQ(
      HeadlessScreenInfo::FromString("{ label='primary screen'}").value()[0],
      HeadlessScreenInfo({.label = "primary screen"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label='my \\'quoted\\' screen'}")
                .value()[0],
            HeadlessScreenInfo({.label = "my 'quoted' screen"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label='\\'quoted\\' screen'}")
                .value()[0],
            HeadlessScreenInfo({.label = "'quoted' screen"}));

  EXPECT_EQ(
      HeadlessScreenInfo::FromString("{ label='\\'quoted\\''}").value()[0],
      HeadlessScreenInfo({.label = "'quoted'"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label='\\'quoted\\'}").error(),
            "Invalid screen info: '\\'quoted\\'");
}

TEST(HeadlessScreenInfoTest, MultipleScreens) {
  // Explicit screen origin results in overlapped screens.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{0,0 600x800}").value(),
              testing::ElementsAre(
                  HeadlessScreenInfo(),
                  HeadlessScreenInfo({.bounds = gfx::Rect(600, 800)})));

  // Default screen origin results in side by side screens.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{}").value(),
              testing::ElementsAre(
                  HeadlessScreenInfo(),
                  HeadlessScreenInfo({.bounds = gfx::Rect(800, 0, 800, 600)})));

  // Screen info separators.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{}").value(),
              testing::SizeIs(2));

  EXPECT_THAT(HeadlessScreenInfo::FromString("{} {}").value(),
              testing::SizeIs(2));

  EXPECT_THAT(HeadlessScreenInfo::FromString("{},{}").value(),
              testing::SizeIs(2));

  EXPECT_THAT(HeadlessScreenInfo::FromString("{} , {}").value(),
              testing::SizeIs(2));

  // Malformed.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{").error(),
              "Invalid screen info: {");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{ xyz").error(),
              "Invalid screen info: { xyz");

  EXPECT_THAT(HeadlessScreenInfo::FromString("xyz{}").error(),
              "Invalid screen info: xyz{}");

  EXPECT_THAT(HeadlessScreenInfo::FromString("xyz {}").error(),
              "Invalid screen info: xyz {}");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{}xyz").error(),
              "Invalid screen info: xyz");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{} xyz }").error(),
              "Invalid screen info: xyz }");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ {}").error(),
              "Invalid screen info: {");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{} }").error(),
              "Invalid screen info: }");
}

TEST(HeadlessScreenInfoTest, WorkArea) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{ workAreaLeft=100 }").value()[0],
            HeadlessScreenInfo(
                {.work_area_insets = gfx::Insets::TLBR(0, 100, 0, 0)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ workAreaRight=100 }").value()[0],
            HeadlessScreenInfo(
                {.work_area_insets = gfx::Insets::TLBR(0, 0, 0, 100)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ workAreaTop=100 }").value()[0],
            HeadlessScreenInfo(
                {.work_area_insets = gfx::Insets::TLBR(100, 0, 0, 0)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ workAreaBottom=100 }").value()[0],
            HeadlessScreenInfo(
                {.work_area_insets = gfx::Insets::TLBR(0, 0, 100, 0)}));

  EXPECT_EQ(
      HeadlessScreenInfo::FromString("{ workAreaLeft=100 workAreaRight=100"
                                     " workAreaTop=100 workAreaBottom=100 }")
          .value()[0],
      HeadlessScreenInfo(
          {.work_area_insets = gfx::Insets::TLBR(100, 100, 100, 100)}));

  // Malformed.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{ workAreaLeft=abc }").error(),
              "Invalid work area inset: abc");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ workAreaRight=abc }").error(),
              "Invalid work area inset: abc");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ workAreaTop=abc }").error(),
              "Invalid work area inset: abc");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ workAreaBottom=abc }").error(),
              "Invalid work area inset: abc");

  // Negative.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{ workAreaLeft=-42 }").error(),
              "Invalid work area inset: -42");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ workAreaRight=-42 }").error(),
              "Invalid work area inset: -42");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ workAreaTop=-42 }").error(),
              "Invalid work area inset: -42");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ workAreaBottom=-42 }").error(),
              "Invalid work area inset: -42");
}

TEST(HeadlessScreenInfoTest, Rotation) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{ rotation=0 }").value()[0],
            HeadlessScreenInfo({.rotation = 0}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ rotation=90 }").value()[0],
            HeadlessScreenInfo({.rotation = 90}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ rotation=180 }").value()[0],
            HeadlessScreenInfo({.rotation = 180}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ rotation=270 }").value()[0],
            HeadlessScreenInfo({.rotation = 270}));

  // Invalid.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{ rotation=abc }").error(),
              "Invalid rotation: abc");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ rotation=-42 }").error(),
              "Invalid rotation: -42");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ rotation=42 }").error(),
              "Invalid rotation: 42");
}

}  // namespace
}  // namespace headless
