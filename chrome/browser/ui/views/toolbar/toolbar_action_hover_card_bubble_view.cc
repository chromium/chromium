// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_bubble_view.h"

#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace {

using HoverCardState = ToolbarActionViewController::HoverCardState;

// Hover card fixed width. Toolbar actions are not visible when window is too
// small to display them, therefore hover cards wouldn't be displayed if the
// window is not big enough.
constexpr int kHoverCardWidth = 240;

// Hover card margins.
// TODO(crbug.com/1351778): Move to a base hover card class.
constexpr int kHorizontalMargin = 18;
constexpr int kVerticalMargin = 10;

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
      break;
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
      break;
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
      break;
  }
  return l10n_util::GetStringUTF16(text_id);
}

// Label that renders its background in a solid color. Placed in front of a
// normal label either by being later in the draw order or on a layer, it can
// be used to animate a fade-out.
class SolidLabel : public views::Label {
 public:
  METADATA_HEADER(SolidLabel);
  using Label::Label;
  SolidLabel() = default;
  ~SolidLabel() override = default;

 protected:
  // views::Label:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    canvas->DrawColor(GetBackgroundColor());
  }
};

BEGIN_METADATA(SolidLabel, views::Label)
END_METADATA

}  // namespace

// ToolbarActionHoverCardBubbleView::FadeLabel:
// ----------------------------------------------------------

// This view overlays and fades out an old version of the text of a label,
// while displaying the new text underneath. It is used to fade out the old
// value of the title and domain labels on the hover card when the tab switches
// or the tab title changes.
// TODO(crbug.com/1354321): ToolbarActionHoverCarBubbleView has the same
// FadeLabel. Move it to its own shared file.
class ToolbarActionHoverCardBubbleView::FadeLabel : public views::View {
 public:
  explicit FadeLabel(int context) {
    primary_label_ = AddChildView(std::make_unique<views::Label>(
        std::u16string(), context, views::style::STYLE_PRIMARY));
    primary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    primary_label_->SetVerticalAlignment(gfx::ALIGN_TOP);
    primary_label_->SetMultiLine(true);

    label_fading_out_ = AddChildView(std::make_unique<SolidLabel>(
        std::u16string(), context, views::style::STYLE_PRIMARY));
    label_fading_out_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_fading_out_->SetVerticalAlignment(gfx::ALIGN_TOP);
    label_fading_out_->SetMultiLine(true);
    label_fading_out_->GetViewAccessibility().OverrideIsIgnored(true);

    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  ~FadeLabel() override = default;

  void SetText(std::u16string text) {
    label_fading_out_->SetText(primary_label_->GetText());
    primary_label_->SetText(text);
  }

  // Sets the fade-out of the label as `percent` in the range [0, 1]. Since
  // FadeLabel is designed to mask new text with the old and then fade away, the
  // higher the percentage the less opaque the label.
  void SetFade(double percent) {
    percent_ = std::min(1.0, percent);
    if (percent_ == 1.0)
      label_fading_out_->SetText(std::u16string());
    const SkAlpha alpha = base::saturated_cast<SkAlpha>(
        std::numeric_limits<SkAlpha>::max() * (1.0 - percent_));
    label_fading_out_->SetBackgroundColor(
        SkColorSetA(label_fading_out_->GetBackgroundColor(), alpha));
    label_fading_out_->SetEnabledColor(
        SkColorSetA(label_fading_out_->GetEnabledColor(), alpha));
  }

  void SetBackgroundColorId(ui::ColorId background_color_id) {
    background_color_id_ = background_color_id;
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    if (background_color_id_.has_value()) {
      label_fading_out_->SetBackgroundColor(
          GetColorProvider()->GetColor(background_color_id_.value()));
      SetFade(percent_);
    }
  }

  std::u16string GetText() const { return primary_label_->GetText(); }

 protected:
  // views::View:
  gfx::Size GetMaximumSize() const override {
    return gfx::Tween::SizeValueBetween(percent_,
                                        label_fading_out_->GetPreferredSize(),
                                        primary_label_->GetPreferredSize());
  }

  gfx::Size CalculatePreferredSize() const override {
    return primary_label_->GetPreferredSize();
  }

  gfx::Size GetMinimumSize() const override {
    return primary_label_->GetMinimumSize();
  }

  int GetHeightForWidth(int width) const override {
    return primary_label_->GetHeightForWidth(width);
  }

 private:
  raw_ptr<views::Label> primary_label_;
  raw_ptr<SolidLabel> label_fading_out_;

  double percent_ = 1.0;
  absl::optional<ui::ColorId> background_color_id_;
};

// ToolbarActionHoverCardBubbleView:
// ----------------------------------------------------------

ToolbarActionHoverCardBubbleView::ToolbarActionHoverCardBubbleView(
    ToolbarActionView* action_view)
    : BubbleDialogDelegateView(action_view,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::STANDARD_SHADOW) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  // Remove dialog's default buttons.
  SetButtons(ui::DIALOG_BUTTON_NONE);

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

  corner_radius_ = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);

  // Set up content.
  auto create_label = [](int context, gfx::Insets insets) {
    auto label = std::make_unique<FadeLabel>(context);
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

  title_label_ = AddChildView(
      create_label(CONTEXT_TAB_HOVER_CARD_TITLE,
                   gfx::Insets::VH(kVerticalMargin, kHorizontalMargin)));

  site_access_separator_ = AddChildView(create_separator());
  site_access_title_label_ = AddChildView(
      create_label(CONTEXT_TAB_HOVER_CARD_TITLE,
                   gfx::Insets::TLBR(kVerticalMargin, kHorizontalMargin, 0,
                                     kHorizontalMargin)));
  site_access_description_label_ = AddChildView(
      create_label(views::style::CONTEXT_DIALOG_BODY_TEXT,
                   gfx::Insets::TLBR(0, kHorizontalMargin, kVerticalMargin,
                                     kHorizontalMargin)));

  policy_separator_ = AddChildView(create_separator());
  policy_label_ = AddChildView(
      create_label(views::style::CONTEXT_DIALOG_BODY_TEXT,
                   gfx::Insets::VH(kVerticalMargin, kHorizontalMargin)));

  // Set up widget.
  views::BubbleDialogDelegateView::CreateBubble(this);
  set_adjust_if_offscreen(true);

  GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);

  if (using_rounded_corners())
    GetBubbleFrameView()->SetCornerRadius(corner_radius_.value());

  // Start in the fully "faded-in" position so that whatever text we initially
  // display is visible.
  SetTextFade(1.0);
}

void ToolbarActionHoverCardBubbleView::UpdateCardContent(
    const ToolbarActionViewController* action_controller,
    content::WebContents* web_contents) {
  title_label_->SetText(action_controller->GetActionName());

  HoverCardState state = action_controller->GetHoverCardState(web_contents);
  bool show_site_access_labels =
      state.site_access !=
      HoverCardState::SiteAccess::kExtensionDoesNotWantAccess;
  bool show_policy_label = state.policy != HoverCardState::AdminPolicy::kNone;

  site_access_separator_->SetVisible(show_site_access_labels);
  site_access_title_label_->SetVisible(show_site_access_labels);
  site_access_description_label_->SetVisible(show_site_access_labels);
  if (show_site_access_labels) {
    site_access_title_label_->SetText(GetSiteAccessTitle(state.site_access));
    site_access_description_label_->SetText(GetSiteAccessDescription(
        state.site_access, GetCurrentHost(web_contents)));
  }

  policy_separator_->SetVisible(show_policy_label);
  policy_label_->SetVisible(show_policy_label);
  if (show_policy_label)
    policy_label_->SetText(GetPolicyText(state.policy));
}

void ToolbarActionHoverCardBubbleView::SetTextFade(double percent) {
  title_label_->SetFade(percent);
  site_access_title_label_->SetFade(percent);
  site_access_description_label_->SetFade(percent);
  policy_label_->SetFade(percent);
}

std::u16string ToolbarActionHoverCardBubbleView::GetTitleTextForTesting()
    const {
  return title_label_->GetText();
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

void ToolbarActionHoverCardBubbleView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();

  // Bubble closes if the theme changes to the point where the border has to be
  // regenerated. See crbug.com/1140256
  if (!using_rounded_corners()) {
    GetWidget()->Close();
    return;
  }
}

ToolbarActionHoverCardBubbleView::~ToolbarActionHoverCardBubbleView() = default;

BEGIN_METADATA(ToolbarActionHoverCardBubbleView,
               views::BubbleDialogDelegateView)
END_METADATA
