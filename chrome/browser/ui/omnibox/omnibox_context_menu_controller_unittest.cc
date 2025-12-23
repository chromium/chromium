// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/aim_tools_and_models.pb.h"

TEST(OmniboxContextMenuControllerTest, IsCommandIdEnabledHelper_InitialState) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  EXPECT_TRUE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
  EXPECT_TRUE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_FILE,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
  EXPECT_TRUE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
  EXPECT_TRUE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
}

TEST(OmniboxContextMenuControllerTest, IsCommandIdEnabledHelper_ImageGenMode) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  EXPECT_TRUE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN, file_infos,
      max_num_files));
  EXPECT_FALSE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_FILE,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN, file_infos,
      max_num_files));
}

TEST(OmniboxContextMenuControllerTest, IsCommandIdEnabledHelper_WithImageFile) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  contextual_search::FileInfo image_file;
  image_file.mime_type = lens::MimeType::kImage;
  file_infos.push_back(image_file);

  EXPECT_FALSE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
  EXPECT_TRUE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
  EXPECT_TRUE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
}

TEST(OmniboxContextMenuControllerTest,
     IsCommandIdEnabledHelper_WithNonImageFile) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  contextual_search::FileInfo pdf_file;
  pdf_file.mime_type = lens::MimeType::kPdf;
  file_infos.push_back(pdf_file);

  EXPECT_FALSE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
  EXPECT_TRUE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
}

TEST(OmniboxContextMenuControllerTest, IsCommandIdEnabledHelper_MaxFiles) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;
  contextual_search::FileInfo image_file;
  image_file.mime_type = lens::MimeType::kImage;

  for (int i = 0; i < max_num_files; ++i) {
    file_infos.push_back(image_file);
  }
  EXPECT_FALSE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
  EXPECT_FALSE(OmniboxContextMenuController::IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_FILE,
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, file_infos,
      max_num_files));
}
