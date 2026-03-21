// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::HighlightPathGenerator>
GetListItemHighlightPathGenerator() {
  return std::make_unique<views::RoundRectHighlightPathGenerator>(
      /*insets=*/gfx::Insets(0),
      /*corner_radius=*/projects_panel::kListItemCornerRadius);
}

}  // namespace

namespace projects_panel {

bool IsProjectsPanelVisibleForProfile(Profile* profile) {
  return tab_groups::IsProjectsPanelFeatureEnabled() &&
         tab_groups::SavedTabGroupUtils::IsEnabledForProfile(profile);
}

void ConfigureInkDropForButton(views::Button* view) {
  auto* ink_drop = views::InkDrop::Get(view);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->SetLayerRegion(views::LayerRegion::kBelow);
  ink_drop->SetBaseColor(kColorProjectsPanelButtonHoverBackground);
  ink_drop->GetInkDrop()->SetHoverHighlightFadeDuration(
      projects_panel::kListItemHoverFadeAnimationDuration);
  ink_drop->SetHighlightOpacity(1.0f);
  views::HighlightPathGenerator::Install(view,
                                         GetListItemHighlightPathGenerator());
  views::FocusRing::Install(view);
  views::FocusRing::Get(view)->SetPathGenerator(
      GetListItemHighlightPathGenerator());
  views::FocusRing::Get(view)->SetHaloInset(
      projects_panel::kListItemFocusRingHaloInset);
}

const gfx::VectorIcon& GetIconForThreadType(
    contextual_tasks::ThreadType thread_type) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (thread_type) {
    case contextual_tasks::ThreadType::kAiMode:
      return vector_icons::kGoogleGLogoMonochromeIcon;
    case contextual_tasks::ThreadType::kGemini:
      return vector_icons::kGoogleAgentspaceMonochromeLogo25Icon;
    case contextual_tasks::ThreadType::kUnknown:
      NOTREACHED();
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kChatSparkIcon;
}

bool IsFirstFocusableViewInPanel(views::View* view) {
  return view->GetProperty(views::kElementIdentifierKey) ==
         kProjectsPanelButtonElementId;
}

}  // namespace projects_panel
