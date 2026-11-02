// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/input_state_model_routing.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/input_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/model_config.pb.h"
#include "third_party/omnibox_proto/tool_config.pb.h"
#include "url/gurl.h"

namespace contextual_search {
namespace {

using ::omnibox::ModelMode;
using ::omnibox::ToolMode;

FileInfo Image() {
  FileInfo info;
  info.mime_type = lens::MimeType::kImage;
  return info;
}

FileInfo Pdf() {
  FileInfo info;
  info.mime_type = lens::MimeType::kPdf;
  return info;
}

FileInfo Tab() {
  FileInfo info;
  info.tab_url = GURL("https://example.com");
  return info;
}

FileInfo Drive() {
  FileInfo info;
  info.mime_type = lens::MimeType::kPdf;
  info.input_data = std::make_unique<lens::ContextualInputData>();
  info.input_data->drive_id = "drive-file-id";
  return info;
}

AttachmentCounts Counts(int images, int files, int tabs = 0, int drive = 0) {
  return {.images = images, .files = files, .tabs = tabs, .drive = drive};
}

TEST(InputStateModelRoutingTest, CountAttachments) {
  std::vector<FileInfo> files;
  files.push_back(Image());
  files.push_back(Image());
  files.push_back(Pdf());
  files.push_back(Tab());
  files.push_back(Drive());
  files.push_back(Image());
  files.back().is_superceded = true;
  EXPECT_EQ(CountAttachments(files, /*pending_tab_count=*/1),
            Counts(/*images=*/2, /*files=*/1, /*tabs=*/2, /*drive=*/1));
}

TEST(InputStateModelRoutingTest, ParseSrpInputMode) {
  EXPECT_EQ(ParseSrpInputMode("none"), SrpInputMode::kNone);
  EXPECT_EQ(ParseSrpInputMode("single_image"), SrpInputMode::kSingleImage);
  EXPECT_EQ(ParseSrpInputMode("multi_image"), SrpInputMode::kMultiImage);
  EXPECT_EQ(ParseSrpInputMode("any_combination"),
            SrpInputMode::kAnyCombination);
  EXPECT_FALSE(ParseSrpInputMode("everything").has_value());
}

TEST(InputStateModelRoutingTest, SrpInputModeFromFeatureParams) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kDynamicAimFulfillment, {});
    EXPECT_EQ(SrpInputModeFromFeatureParams(), SrpInputMode::kSingleImage);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kDynamicAimFulfillment,
        {{"srp_input_mode", "any_combination"}});
    EXPECT_EQ(SrpInputModeFromFeatureParams(), SrpInputMode::kAnyCombination);
  }
}

// Which attachments can go to SRP in each mode.
TEST(InputStateModelRoutingTest, AttachmentsCanGoToSrpPerMode) {
  struct Row {
    AttachmentCounts counts;
    bool none, single, multi, any;
  };
  const Row rows[] = {
      {Counts(0, 0), true, true, true, true},            // No attachments.
      {Counts(1, 0), false, true, true, true},           // 1 image.
      {Counts(3, 0), false, false, true, true},          // 2+ images.
      {Counts(0, 2), false, false, false, true},         // Files.
      {Counts(3, 2), false, false, false, true},         // Images + files.
      {Counts(0, 0, 1), false, false, false, false},     // Tab.
      {Counts(1, 0, 0, 1), false, false, false, false},  // Image + Drive.
  };
  for (const Row& row : rows) {
    SCOPED_TRACE(testing::Message()
                 << "images=" << row.counts.images
                 << " files=" << row.counts.files << " tabs=" << row.counts.tabs
                 << " drive=" << row.counts.drive);
    auto can_go = [&](SrpInputMode mode) {
      return AttachmentsCanGoToSrp(row.counts,
                                   EligibleAttachmentsForMode(mode));
    };
    EXPECT_EQ(can_go(SrpInputMode::kNone), row.none);
    EXPECT_EQ(can_go(SrpInputMode::kSingleImage), row.single);
    EXPECT_EQ(can_go(SrpInputMode::kMultiImage), row.multi);
    EXPECT_EQ(can_go(SrpInputMode::kAnyCombination), row.any);
  }
}

omnibox::ToolConfig MakeTool(
    ToolMode tool,
    std::vector<std::pair<std::string, std::string>> params) {
  omnibox::ToolConfig config;
  config.set_tool(tool);
  for (const auto& [key, value] : params) {
    auto* param = config.add_aim_url_params();
    param->set_param_key(key);
    param->set_param_value(value);
  }
  return config;
}

omnibox::ModelConfig MakeModel(
    ModelMode model,
    std::vector<std::pair<std::string, std::string>> params) {
  omnibox::ModelConfig config;
  config.set_model(model);
  for (const auto& [key, value] : params) {
    auto* param = config.add_aim_url_params();
    param->set_param_key(key);
    param->set_param_value(value);
  }
  return config;
}

// A config where AI Mode-only tools carry `udm=50` and Create Images doesn't.
omnibox::InputState MakeInputState() {
  omnibox::InputState state;
  state.tool_configs = {
      MakeTool(ToolMode::TOOL_MODE_IMAGE_GEN, {{"imgn", "1"}}),
      MakeTool(ToolMode::TOOL_MODE_CANVAS, {{"rc", "1"}, {"udm", "50"}}),
  };
  state.model_configs = {
      MakeModel(ModelMode::MODEL_MODE_GEMINI_REGULAR, {{"udm", "50"}}),
      MakeModel(ModelMode::MODEL_MODE_GEMINI_PRO, {{"nem", "143"}}),
  };
  state.allowed_models = {ModelMode::MODEL_MODE_GEMINI_REGULAR,
                          ModelMode::MODEL_MODE_GEMINI_PRO};
  return state;
}

FulfillmentDecision Compute(const omnibox::InputState& state,
                            AttachmentCounts counts = {},
                            bool aim_mode_active = false,
                            bool has_valid_config = true) {
  return ComputeFulfillment(
      state, aim_mode_active, counts,
      EligibleAttachmentsForMode(SrpInputMode::kSingleImage), has_valid_config);
}

TEST(InputStateModelRoutingTest, NothingSelectedRoutesToSrp) {
  FulfillmentDecision result = Compute(MakeInputState());
  EXPECT_FALSE(result.will_fulfill_in_aim);
  EXPECT_EQ(result.model, ModelMode::MODEL_MODE_UNSPECIFIED);
  EXPECT_TRUE(result.url_params.empty());
}

TEST(InputStateModelRoutingTest, ColdStartRoutesToAim) {
  FulfillmentDecision result =
      Compute(omnibox::InputState(), {}, /*aim_mode_active=*/false,
              /*has_valid_config=*/false);
  EXPECT_TRUE(result.will_fulfill_in_aim);
  EXPECT_EQ(result.url_params, UrlParams({{"udm", "50"}}));
}

TEST(InputStateModelRoutingTest, IneligibleAttachmentsRouteToAim) {
  // Going to AIM shows the default model and adds its params.
  FulfillmentDecision result =
      Compute(MakeInputState(), Counts(/*images=*/2, 0));
  EXPECT_TRUE(result.will_fulfill_in_aim);
  EXPECT_EQ(result.model, ModelMode::MODEL_MODE_GEMINI_REGULAR);
  EXPECT_EQ(result.url_params, UrlParams({{"udm", "50"}}));
}

TEST(InputStateModelRoutingTest, ToolsRouteByTheirParams) {
  omnibox::InputState state = MakeInputState();

  state.active_tool = ToolMode::TOOL_MODE_IMAGE_GEN;
  FulfillmentDecision image_gen = Compute(state);
  EXPECT_FALSE(image_gen.will_fulfill_in_aim);
  EXPECT_EQ(image_gen.url_params, UrlParams({{"imgn", "1"}}));

  state.active_tool = ToolMode::TOOL_MODE_CANVAS;
  FulfillmentDecision canvas = Compute(state);
  EXPECT_TRUE(canvas.will_fulfill_in_aim);
  EXPECT_EQ(canvas.url_params, UrlParams({{"rc", "1"}, {"udm", "50"}}));

  // A tool without a config goes to AIM.
  state.active_tool = ToolMode::TOOL_MODE_DEEP_SEARCH;
  EXPECT_TRUE(Compute(state).will_fulfill_in_aim);
}

TEST(InputStateModelRoutingTest, ToolWithAttachmentsRoutesToAim) {
  omnibox::InputState state = MakeInputState();
  state.active_tool = ToolMode::TOOL_MODE_IMAGE_GEN;
  const AttachmentCounts one_image = Counts(/*images=*/1, 0);

  FulfillmentDecision result = Compute(state, one_image);
  EXPECT_TRUE(result.will_fulfill_in_aim);
  EXPECT_EQ(result.url_params, UrlParams({{"imgn", "1"}, {"udm", "50"}}));
  // Not controlled by the mode.
  EXPECT_TRUE(ComputeFulfillment(
                  state, /*aim_mode_active=*/false, one_image,
                  EligibleAttachmentsForMode(SrpInputMode::kAnyCombination),
                  /*has_valid_config=*/true)
                  .will_fulfill_in_aim);
}

TEST(InputStateModelRoutingTest, AimModeRoutesToAim) {
  omnibox::InputState state = MakeInputState();
  FulfillmentDecision result = Compute(state, {}, /*aim_mode_active=*/true);
  EXPECT_TRUE(result.will_fulfill_in_aim);
  EXPECT_EQ(result.url_params, UrlParams({{"udm", "50"}}));

  // The chosen model's params apply in AI Mode; `nem` replaces the tool's
  // `udm`.
  state.active_tool = ToolMode::TOOL_MODE_CANVAS;
  state.active_model = ModelMode::MODEL_MODE_GEMINI_PRO;
  result = Compute(state, {}, /*aim_mode_active=*/true);
  EXPECT_EQ(result.url_params, UrlParams({{"rc", "1"}, {"nem", "143"}}));
  state.active_tool = ToolMode::TOOL_MODE_UNSPECIFIED;

  // Outside AI Mode, a model (e.g. shown before an update) doesn't count.
  EXPECT_FALSE(Compute(state).will_fulfill_in_aim);
}

}  // namespace
}  // namespace contextual_search
