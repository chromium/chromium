// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_expand_button.h"

#include "base/functional/callback_helpers.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

class ProjectsPanelRecentThreadsExpandButtonTest : public views::ViewsTestBase {
 public:
  ProjectsPanelRecentThreadsExpandButtonTest() = default;
};

TEST_F(ProjectsPanelRecentThreadsExpandButtonTest, ShowsCorrectTextAndIcon) {
  auto button = std::make_unique<ProjectsPanelRecentThreadsExpandButton>(
      base::DoNothing());

  // Initially the button should be in the collapsed state.
  button->SetExpanded(false);
  EXPECT_EQ(button->title_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(IDS_THREADS_SHOW_MORE));

  ui::ImageModel icon_model = button->icon_view_for_testing()->GetImageModel();
  EXPECT_TRUE(icon_model.IsVectorIcon());
  EXPECT_EQ(icon_model.GetVectorIcon().vector_icon()->name,
            kKeyboardArrowDownChromeRefreshIcon.name);

  // Switch to expanded.
  button->SetExpanded(true);
  EXPECT_EQ(button->title_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(IDS_THREADS_SHOW_LESS));

  icon_model = button->icon_view_for_testing()->GetImageModel();
  EXPECT_TRUE(icon_model.IsVectorIcon());
  EXPECT_EQ(icon_model.GetVectorIcon().vector_icon()->name,
            kKeyboardArrowUpChromeRefreshIcon.name);

  // Switch back to collapsed.
  button->SetExpanded(false);
  EXPECT_EQ(button->title_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(IDS_THREADS_SHOW_MORE));

  icon_model = button->icon_view_for_testing()->GetImageModel();
  EXPECT_TRUE(icon_model.IsVectorIcon());
  EXPECT_EQ(icon_model.GetVectorIcon().vector_icon()->name,
            kKeyboardArrowDownChromeRefreshIcon.name);
}
