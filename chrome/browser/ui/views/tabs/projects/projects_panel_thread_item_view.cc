// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

ProjectsPanelThreadItemView::ProjectsPanelThreadItemView(
    const contextual_tasks::Thread& thread) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(projects_panel::kListItemPadding)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  GetViewAccessibility().SetName(thread.title);

  auto* ink_drop = views::InkDrop::Get(this);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->SetLayerRegion(views::LayerRegion::kBelow);
  ink_drop->SetBaseColor(ui::kColorSysStateHoverOnSubtle);
  ink_drop->SetHighlightOpacity(1.0f);
  views::HighlightPathGenerator::Install(
      this, projects_panel::GetListItemHighlightPathGenerator());
  views::FocusRing::Install(this);
  views::FocusRing::Get(this)->SetPathGenerator(
      projects_panel::GetListItemHighlightPathGenerator());
  views::FocusRing::Get(this)->SetHaloInset(
      projects_panel::kListItemFocusRingHaloInset);

  auto aim_icon = ui::ImageModel::FromVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      vector_icons::kGoogleGLogoMonochromeIcon,
#else
      vector_icons::kChatSparkIcon,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      ui::kColorIcon, projects_panel::kListItemIconSize);

  auto gemini_icon = ui::ImageModel::FromVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      vector_icons::kGoogleAgentspaceMonochromeLogo25Icon,
#else
      vector_icons::kChatSparkIcon,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      ui::kColorIcon, projects_panel::kListItemIconSize);

  switch (thread.type) {
    case contextual_tasks::ThreadType::kAiMode: {
      AddChildView(std::make_unique<views::ImageView>(aim_icon));
      break;
    }
    case contextual_tasks::ThreadType::kGemini: {
      AddChildView(std::make_unique<views::ImageView>(gemini_icon));
      break;
    }
    case contextual_tasks::ThreadType::kUnknown:
      NOTREACHED();
  }

  auto* title = AddChildView(std::make_unique<views::Label>());
  title->SetText(base::UTF8ToUTF16(thread.title));
  title->SetTextStyle(views::style::TextStyle::STYLE_BODY_3);
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  title->SetElideBehavior(gfx::FADE_TAIL);
  title->SetProperty(views::kMarginsKey, projects_panel::kListItemTitlePadding);
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kScaleToMaximum));

  SetProperty(views::kElementIdentifierKey,
              kProjectsPanelThreadListItemViewElementId);
}

ProjectsPanelThreadItemView::~ProjectsPanelThreadItemView() = default;

BEGIN_METADATA(ProjectsPanelThreadItemView)
END_METADATA
