// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_bubble_view.h"

#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

using HoverCardState = ToolbarActionViewController::HoverCardState;

// Hover card fixed width. Toolbar actions are not visible when window is too
// small to display them, therefore hover cards wouldn't be displayed if the
// window is not big enough.
constexpr int kHoverCardWidth = 240;

// Hover card margins.
// TODO(crbug.com/40857356): Move to a base hover card class.
constexpr int kHorizontalMargin = 12;
constexpr int kVerticalMargin = 12;

// Maximum number of lines that a label occupies.
constexpr int kHoverCardLabelMaxLines = 3;

std::u16string GetSiteAccessTitle(
    ToolbarActionViewController::HoverCardState::SiteAccess state) {
  int title_id = -1;
  switch (state) {
    case HoverCardState::SiteAccess::kAllExtensionsAllowed:
    case HoverCardState::SiteAccess::kExtensionHasAccess:
      title_id = IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_TITLE_HAS_ACCESS;
      break;
    case HoverCardState::SiteAccess::kAllExtensionsBlocked:
      title_id = IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_TITLE_BLOCKED_ACCESS;
      break;
    case HoverCardState::SiteAccess::kExtensionRequestsAccess:
      title_id = IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_TITLE_REQUESTS_ACCESS;
      break;
    case HoverCardState::SiteAccess::kExtensionDoesNotWantAccess:
      NOTREACHED();
  }
  return l10n_util::GetStringUTF16(title_id);
}

std::u16string GetSiteAccessDescription(HoverCardState::SiteAccess state,
                                        std::u16string host) {
  int title_id = -1;
  switch (state) {
    case HoverCardState::SiteAccess::kAllExtensionsAllowed:
      title_id =
          IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_DESCRIPTION_ALL_EXTENSIONS_ALLOWED_ACCESS;
      break;
    case HoverCardState::SiteAccess::kAllExtensionsBlocked:
      title_id =
          IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_DESCRIPTION_ALL_EXTENSIONS_BLOCKED_ACCESS;
      break;
    case HoverCardState::SiteAccess::kExtensionHasAccess:
      title_id =
          IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_DESCRIPTION_EXTENSION_HAS_ACCESS;
      break;
    case HoverCardState::SiteAccess::kExtensionRequestsAccess:
      title_id =
          IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_DESCRIPTION_EXTENSION_REQUESTS_ACCESS;
      break;
    case HoverCardState::SiteAccess::kExtensionDoesNotWantAccess:
      NOTREACHED();
  }
  return l10n_util::GetStringFUTF16(title_id, host);
}

std::u16string GetPolicyText(HoverCardState::AdminPolicy state) {
  int text_id = -1;
  switch (state) {
    case HoverCardState::AdminPolicy::kPinnedByAdmin:
      text_id =
          IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_POLICY_LABEL_PINNED_TEXT;
      break;
    case HoverCardState::AdminPolicy::kInstalledByAdmin:
      text_id =
          IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_POLICY_LABEL_INSTALLED_TEXT;
      break;
    case HoverCardState::AdminPolicy::kNone:
      NOTREACHED();
  }
  return l10n_util::GetStringUTF16(text_id);
}

}  // namespace

ToolbarActionHoverCardBubbleView::ToolbarActionHoverCardBubbleView(
    ToolbarActionView* action_view)
    : BubbleDialogDelegateView(action_view,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::STANDARD_SHADOW) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  // Remove dialog's default buttons.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  // Remove the accessible role so that hover cards are not read when they
  // appear because tabs handle accessibility text.
  SetAccessibleWindowRole(ax::mojom::Role::kNone);

  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Set so that when hovering over a toolbar action in a inactive window that
  // window will not become active. Setting this to false creates the need to
  // explicitly hide the hovercard on press, touch, and keyboard events.
  SetCanActivate(false);
#if BUILDFLAG(IS_MAC)
  set_accept_events(false);
#endif

  // Set so that the toolbar action hover card is not focus traversable when
  // keyboard navigating through the tab strip.
  set_focus_traversable_from_anchor_view(false);

  set_fixed_width(kHoverCardWidth);

  // Let anchor point handle its own highlight, since the hover card is the
  // same for multiple anchor points.
  set_highlight_button_when_shown(false);

  // Set up layout.
  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetCollapseMargins(true);

  // Set up content.
  auto create_label = [](int context, int text_style,
                         std::optional<ui::ColorId> color_id,
                         gfx::Insets insets) {
    auto label = std::make_unique<FadeLabelView>(kHoverCardLabelMaxLines,
                                                 context, text_style);
    if (color_id) {
      label->SetEnabledColorId(color_id.value());
    }
    label->SetProperty(views::kMarginsKey, insets);
    label->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kScaleToMaximum));
    return label;
  };

  auto create_separator = []() {
    auto separator = std::make_unique<views::Separator>();
    separator->SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(kVerticalMargin, 0));
    return separator;
  };

  title_label_ = AddChildView(create_label(
      CONTEXT_TAB_HOVER_CARD_TITLE, views::style::STYLE_BODY_3_EMPHASIS,
      /*color_id=*/std::nullopt,
      gfx::Insets::VH(kVerticalMargin, kHorizontalMargin)));
  action_title_label_ = AddChildView(create_label(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_4,
      /*color_id=*/kColorTabHoverCardSecondaryText,
      gfx::Insets::TLBR(0, kHorizontalMargin, kVerticalMargin,
                        kHorizontalMargin)));

  site_access_separator_ = AddChildView(create_separator());
  site_access_title_label_ = AddChildView(create_label(
      CONTEXT_TAB_HOVER_CARD_TITLE, views::style::STYLE_BODY_3_EMPHASIS,
      /*color_id=*/std::nullopt,
      gfx::Insets::TLBR(kVerticalMargin, kHorizontalMargin, 0,
                        kHorizontalMargin)));
  site_access_description_label_ = AddChildView(create_label(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_4,
      /*color_id=*/kColorTabHoverCardSecondaryText,
      gfx::Insets::TLBR(0, kHorizontalMargin, kVerticalMargin,
                        kHorizontalMargin)));

  policy_separator_ = AddChildView(create_separator());
  policy_label_ = AddChildView(create_label(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_4,
      /*color_id=*/kColorTabHoverCardSecondaryText,
      gfx::Insets::VH(kVerticalMargin, kHorizontalMargin)));

  // Set up widget.
  views::BubbleDialogDelegateView::CreateBubble(this);
  set_adjust_if_offscreen(true);

  GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);
  GetBubbleFrameView()->SetCornerRadius(
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh));

  // Start in the fully "faded-in" position so that whatever text we initially
  // display is visible.
  SetTextFade(1.0);
}

void ToolbarActionHoverCardBubbleView::UpdateCardContent(
    const std::u16string& extension_name,
    const std::u16string& action_title,
    ToolbarActionViewController::HoverCardState state,
    content::WebContents* web_contents) {
  title_label_->SetData({extension_name, /*is_filename=*/false});

  // We need to adjust the bottom margin of `title_label_` depending on
  // `action_title_` visibility.
  if (action_title.empty()) {
    title_label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(kVerticalMargin, kHorizontalMargin));
    action_title_label_->SetVisible(false);
  } else {
    title_label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(kVerticalMargin, kHorizontalMargin, 0,
                          kHorizontalMargin));
    action_title_label_->SetData({action_title, /*is_filename=*/false});
    action_title_label_->SetVisible(true);
  }

  bool show_site_access_labels =
      state.site_access !=
      HoverCardState::SiteAccess::kExtensionDoesNotWantAccess;
  bool show_policy_label = state.policy != HoverCardState::AdminPolicy::kNone;

  site_access_separator_->SetVisible(show_site_access_labels);
  site_access_title_label_->SetVisible(show_site_access_labels);
  site_access_description_label_->SetVisible(show_site_access_labels);
  if (show_site_access_labels) {
    site_access_title_label_->SetData(
        {GetSiteAccessTitle(state.site_access), /*is_filename=*/false});
    site_access_description_label_->SetData(
        {GetSiteAccessDescription(state.site_access,
                                  GetCurrentHost(web_contents)),
         /*is_filename=*/false});
  }

  policy_separator_->SetVisible(show_policy_label);
  policy_label_->SetVisible(show_policy_label);
  if (show_policy_label)
    policy_label_->SetData({GetPolicyText(state.policy), false});
}

void ToolbarActionHoverCardBubbleView::SetTextFade(double percent) {
  title_label_->SetFade(percent);
  action_title_label_->SetFade(percent);
  site_access_title_label_->SetFade(percent);
  site_access_description_label_->SetFade(percent);
  policy_label_->SetFade(percent);
}

std::u16string ToolbarActionHoverCardBubbleView::GetTitleTextForTesting()
    const {
  return title_label_->GetText();
}

std::u16string ToolbarActionHoverCardBubbleView::GetActionTitleTextForTesting()
    const {
  return action_title_label_->GetText();
}

std::u16string
ToolbarActionHoverCardBubbleView::GetSiteAccessTitleTextForTesting() const {
  return site_access_title_label_->GetText();
}

std::u16string
ToolbarActionHoverCardBubbleView::GetSiteAccessDescriptionTextForTesting()
    const {
  return site_access_description_label_->GetText();
}

bool ToolbarActionHoverCardBubbleView::IsActionTitleVisible() const {
  return action_title_label_->GetVisible();
}

bool ToolbarActionHoverCardBubbleView::IsSiteAccessSeparatorVisible() const {
  return site_access_separator_->GetVisible();
}

bool ToolbarActionHoverCardBubbleView::IsSiteAccessTitleVisible() const {
  return site_access_title_label_->GetVisible();
}

bool ToolbarActionHoverCardBubbleView::IsSiteAccessDescriptionVisible() const {
  return site_access_description_label_->GetVisible();
}

bool ToolbarActionHoverCardBubbleView::IsPolicySeparatorVisible() const {
  return policy_separator_->GetVisible();
}

bool ToolbarActionHoverCardBubbleView::IsPolicyLabelVisible() const {
  return policy_label_->GetVisible();
}

ToolbarActionHoverCardBubbleView::~ToolbarActionHoverCardBubbleView() = default;

BEGIN_METADATA(ToolbarActionHoverCardBubbleView)
END_METADATA
