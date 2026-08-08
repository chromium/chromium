// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fusebox_action_mojo_utils.h"

#include <sstream>

#include "components/omnibox/browser/fusebox_action_mojo_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"

namespace fusebox_action {
namespace {

TEST(FuseboxActionMojoUtilsTest, ConvertsAllProtoFieldsToMojo) {
  omnibox::SuggestTemplateInfo::FuseboxAction proto;
  proto.set_preselected_tool(omnibox::TOOL_MODE_CANVAS);
  proto.set_preferred_inventory(
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT);

  mojom::FuseboxActionPtr mojo_action = SyncFuseboxActionProtoToMojo(proto);
  ASSERT_TRUE(mojo_action);

  EXPECT_EQ(mojo_action->preselected_tool, omnibox::TOOL_MODE_CANVAS);
  EXPECT_EQ(mojo_action->preferred_inventory,
            omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT);
  EXPECT_FALSE(mojo_action->preselected_model);
}

TEST(FuseboxActionMojoUtilsTest, DebugPrintFuseboxAction) {
  auto action = mojom::FuseboxAction::New();
  action->preselected_tool = omnibox::TOOL_MODE_CANVAS;
  action->preferred_inventory =
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT;

  std::ostringstream os;
  mojom::PrintTo(*action, &os);
  EXPECT_EQ(os.str(),
            "FuseboxAction{\n"
            "  preselected_tool: 2,\n"
            "  preferred_inventory: 0,\n"
            "  preselected_model: null,\n"
            "}");
}

}  // namespace
}  // namespace fusebox_action
