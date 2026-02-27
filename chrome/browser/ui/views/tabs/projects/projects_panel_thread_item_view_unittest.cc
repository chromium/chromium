// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"

#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace {

contextual_tasks::Thread CreateThread(const std::string& title) {
  return contextual_tasks::Thread(contextual_tasks::ThreadType::kAiMode,
                                  /*server_id=*/"", title,
                                  /*conversation_turn_id=*/"");
}

contextual_tasks::Thread CreateGeminiThread() {
  return contextual_tasks::Thread(contextual_tasks::ThreadType::kGemini,
                                  /*server_id=*/"", "Gemini Thread");
}

}  // namespace

class ProjectsPanelThreadItemViewTest : public views::ViewsTestBase {};

TEST_F(ProjectsPanelThreadItemViewTest, DisplaysIconAndTitle) {
  const auto thread = CreateThread("Thread 1");
  auto thread_item_view = std::make_unique<ProjectsPanelThreadItemView>(thread);

  // Check that the item has an image, label, and ink drop.
  ASSERT_EQ(3u, thread_item_view->children().size());

  views::ImageView* image_view =
      static_cast<views::ImageView*>(thread_item_view->children()[1]);
  EXPECT_TRUE(image_view);

  // Check that the image view has the correct icon.
  EXPECT_EQ(ui::ImageModel::FromVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                vector_icons::kGoogleGLogoMonochromeIcon,
#else
                vector_icons::kChatSparkIcon,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
                ui::kColorIcon, projects_panel::kListItemIconSize),
            image_view->GetImageModel());

  views::Label* label =
      static_cast<views::Label*>(thread_item_view->children()[2]);
  EXPECT_TRUE(label);
  EXPECT_EQ(base::UTF8ToUTF16(thread.title), label->GetText());
}

TEST_F(ProjectsPanelThreadItemViewTest, DisplaysGeminiIconAndTitle) {
  const auto gemini_thread = CreateGeminiThread();
  auto thread_item_view =
      std::make_unique<ProjectsPanelThreadItemView>(gemini_thread);

  // Check that the item has an image, label, and ink drop.
  ASSERT_EQ(3u, thread_item_view->children().size());

  views::ImageView* image_view =
      static_cast<views::ImageView*>(thread_item_view->children()[1]);
  EXPECT_TRUE(image_view);

  // Check that the image view has the correct icon.
  EXPECT_EQ(ui::ImageModel::FromVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                vector_icons::kGoogleAgentspaceMonochromeLogo25Icon,
#else
                vector_icons::kChatSparkIcon,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
                ui::kColorIcon, projects_panel::kListItemIconSize),
            image_view->GetImageModel());

  views::Label* label =
      static_cast<views::Label*>(thread_item_view->children()[2]);
  EXPECT_TRUE(label);
  EXPECT_EQ(base::UTF8ToUTF16(gemini_thread.title), label->GetText());
}
