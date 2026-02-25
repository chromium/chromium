// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include <memory>
#include <utility>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/test_omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

class OmniboxContextMenuControllerTest : public testing::Test {
 public:
  OmniboxContextMenuControllerTest() {
    file_selector_ =
        std::make_unique<TestOmniboxPopupFileSelector>(gfx::NativeWindow());
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    controller_ = std::make_unique<OmniboxContextMenuController>(
        file_selector_.get(), web_contents_.get());
  }

  OmniboxContextMenuController* controller() { return controller_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  TestingProfile profile_;

  std::unique_ptr<OmniboxPopupFileSelector> file_selector_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<OmniboxContextMenuController> controller_;
};

TEST_F(OmniboxContextMenuControllerTest,
       IsCommandIdEnabledHelper_InitialState) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_FILE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
}

TEST_F(OmniboxContextMenuControllerTest,
       IsCommandIdEnabledHelper_ImageGenMode) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_FILE, omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
      file_infos, max_num_files, OmniboxPopupState::kNone));
}

TEST_F(OmniboxContextMenuControllerTest,
       IsCommandIdEnabledHelper_WithImageFile) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  contextual_search::FileInfo image_file;
  image_file.mime_type = lens::MimeType::kImage;
  file_infos.push_back(image_file);

  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
}

TEST_F(OmniboxContextMenuControllerTest,
       IsCommandIdEnabledHelper_WithNonImageFile) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  contextual_search::FileInfo pdf_file;
  pdf_file.mime_type = lens::MimeType::kPdf;
  file_infos.push_back(pdf_file);

  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
}

TEST_F(OmniboxContextMenuControllerTest, IsCommandIdEnabledHelper_MaxFiles) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;
  contextual_search::FileInfo image_file;
  image_file.mime_type = lens::MimeType::kImage;

  for (int i = 0; i < max_num_files; ++i) {
    file_infos.push_back(image_file);
  }
  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_FILE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
}
