// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"
#include "base/json/json_reader.h"
#include "components/arc/input_overlay/actions/action_tap_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace arc {

class InputOverlayResourcesUtilTest : public testing::Test {
 protected:
  InputOverlayResourcesUtilTest() = default;
};

constexpr const char kValidJson[] =
    R"json({
      "tap": {
        "keyboard": [
          {
            "name": "Fight",
            "key": "KeyA",
            "location": {
              "position": [
                {
                  "anchor": [
                    0,
                    0
                  ],
                  "anchor_to_target": [
                    0.3,
                    0.5
                  ]
                },
                {
                  "anchor": [
                    0,
                    0
                  ],
                  "anchor_to_target": [
                    0.5,
                    0.5
                  ]
                }
              ]
            }
          },
          {
            "name": "Run",
            "key": "KeyB",
            "location": {
              "position": [
                {
                  "anchor_to_target": [
                    0.8,
                    0.8
                  ]
                }
              ]
            }
          }
        ]
      }
    })json";

constexpr const char kHalfValidJson[] =
    R"json({
      "tap": {
        "keyboard": [
          {
            "name": "Fight",
            "key": "KeyA",
            "location": {
              "position": [
                {
                  "anchor": [
                    0,
                    0
                  ],
                  "anchor_to_target": [
                    0.5,
                    0.5
                  ]
                }
              ]
            }
          },
          {
            "name": "Run",
            "key": "Key_B",
            "location": {
              "position": [
                {
                  "anchor_to_target": [
                    0.8,
                    0.8
                  ]
                }
              ]
            }
          }
        ]
      }
    })json";

constexpr const char kLocationValidJson[] =
    R"json({
        "position": [
          {
            "anchor": [
              0,
              0
            ],
            "anchor_to_target": [
              0.5,
              0.5
            ]
          }
        ]
    })json";

constexpr const char kEmptyJson[] =
    R"json({
    })json";

TEST(InputOverlayResourcesUtilTest, TestParseLocation) {
  // Check whether Position is parsed correctly.
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kLocationValidJson);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  auto result = ParseLocation(json_value.value.value());
  EXPECT_TRUE(result && result->size() == 1);

  json_value = base::JSONReader::ReadAndReturnValueWithError(kEmptyJson);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  result = ParseLocation(json_value.value.value());
  EXPECT_FALSE(result);
}

TEST(InputOverlayResourcesUtilTest, TestParseJsonToActions) {
  // Check whether ActionTapKey is parsed correctly.
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJson);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  aura::test::TestWindowDelegate dummy_delegate;
  auto window = base::WrapUnique(aura::test::CreateTestWindowWithDelegate(
      &dummy_delegate, 11, gfx::Rect(200, 400), nullptr));
  auto result = ParseJsonToActions(window.get(), json_value.value.value());
  EXPECT_TRUE(result && result->size() == 2);
  auto* action = (input_overlay::ActionTapKey*)result->at(0).get();
  EXPECT_EQ(ui::DomCode::US_A, action->key());
  EXPECT_TRUE(result->at(0)->locations().size() == 2);
  auto* pos = result->at(0)->locations().at(0).get();
  EXPECT_TRUE(std::abs(0 - pos->anchor().x()) < 0.000001);
  EXPECT_TRUE(std::abs(0 - pos->anchor().y()) < 0.000001);
  EXPECT_TRUE(std::abs(0.3 - pos->anchor_to_target().x()) < 0.0000001);
  EXPECT_TRUE(std::abs(0.5 - pos->anchor_to_target().y()) < 0.0000001);
  pos = result->at(0)->locations().at(1).get();
  EXPECT_TRUE(std::abs(0 - pos->anchor().x()) < 0.000001);
  EXPECT_TRUE(std::abs(0 - pos->anchor().y()) < 0.000001);
  EXPECT_TRUE(std::abs(0.5 - pos->anchor_to_target().x()) < 0.0000001);
  EXPECT_TRUE(std::abs(0.5 - pos->anchor_to_target().y()) < 0.0000001);
  action = (input_overlay::ActionTapKey*)result->at(1).get();
  EXPECT_EQ(ui::DomCode::US_B, action->key());

  // Parse half valid Json, so only one action is parsed correctly.
  json_value = base::JSONReader::ReadAndReturnValueWithError(kHalfValidJson);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  result = ParseJsonToActions(window.get(), json_value.value.value());
  EXPECT_TRUE(result && result->size() == 1);

  // Parse empty Json, it should receive empty result.
  json_value = base::JSONReader::ReadAndReturnValueWithError(kEmptyJson);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  result = ParseJsonToActions(window.get(), json_value.value.value());
  EXPECT_FALSE(result);
}

}  // namespace arc
