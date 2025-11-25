// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_icon.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kAfterIconPadding = 8;
constexpr int kAfterTitlePadding = 4;
constexpr int kAfterAlertIndicatorPadding = 4;
constexpr int kTitleNoCloseButtonRightPadding = 11;
constexpr int kTitleHeight = 18;
// TODO(crbug.com/454686636): Determine what this min width should be.
constexpr int kVerticalTabExpandedMinWidth = 50;

class VerticalTabTitle : public views::Label {
  METADATA_HEADER(VerticalTabTitle, views::Label)
 public:
  VerticalTabTitle() {
    SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    SetElideBehavior(gfx::FADE_TAIL);
    SetHandlesTooltips(false);
    SetBackgroundColor(SK_ColorTRANSPARENT);
    SetProperty(views::kElementIdentifierKey, kVerticalTabTitleElementId);
  }
};

BEGIN_METADATA(VerticalTabTitle)
END_METADATA
}  // namespace

VerticalTabView::VerticalTabView(TabCollectionNode* collection_node)
    : collection_node_(collection_node),
      icon_(AddChildView(std::make_unique<VerticalTabIcon>(
          *collection_node_->data()->get_tab()))),
      title_(AddChildView(std::make_unique<VerticalTabTitle>())),
      alert_indicator_(
          AddChildView(std::make_unique<AlertIndicatorButton>(this))),
      close_button_(AddChildView(std::make_unique<TabCloseButton>(
          // TODO(crbug.com/460536208): Implement callbacks.
          views::Button::PressedCallback(
              base::DoNothingAs<void(const ui::Event&)>()),
          base::DoNothingAs<void(views::View*, const ui::MouseEvent&)>()))) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabView::ResetCollectionNode, base::Unretained(this)));
  data_changed_subscription_ =
      collection_node_->RegisterDataChangedCallback(base::BindRepeating(
          &VerticalTabView::OnDataChanged, base::Unretained(this)));
  // TODO(crbug.com/444283717): Separate pinned and unpinned tabs.

  OnDataChanged();
}

VerticalTabView::~VerticalTabView() = default;

views::ProposedLayout VerticalTabView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  // TODO(crbug.com/444283717): Separate pinned and unpinned tabs.
  views::ProposedLayout layouts;

  // TODO(crbug.com/454686636): Handle collapsed state.
  const int width =
      std::max(kVerticalTabExpandedMinWidth, size_bounds.width().value_or(0));
  const int height = GetLayoutConstant(VERTICAL_TAB_HEIGHT);
  layouts.host_size = gfx::Size(width, height);

  const gfx::Rect contents_rect = GetContentsBounds();

  // TabIcon calculates its preferred size by starting with the favicon size,
  // and enlarging it to fit the attention indicator and discard ring. Use its
  // preferred size instead of gfx::kFaviconSize.
  gfx::Size icon_size = icon_->GetPreferredSize();
  const int icon_padding = (height - icon_size.height()) / 2;
  const gfx::Rect icon_bounds(contents_rect.x() + icon_padding,
                              contents_rect.y() + icon_padding,
                              icon_size.width(), icon_size.height());
  layouts.child_layouts.emplace_back(icon_.get(), icon_->GetVisible(),
                                     icon_bounds);

  gfx::Size close_button_size = close_button_->GetPreferredSize();
  const int close_button_padding = (height - close_button_size.height()) / 2;
  const gfx::Rect close_button_bounds(
      contents_rect.right() - close_button_padding - close_button_size.width(),
      contents_rect.y() + close_button_padding, close_button_size.width(),
      close_button_size.height());
  layouts.child_layouts.emplace_back(
      close_button_.get(), close_button_->GetVisible(), close_button_bounds);
  // The close button icon is centered within the close button.
  const int close_button_icon_x =
      close_button_bounds.x() +
      (close_button_bounds.width() - GetLayoutConstant(TAB_CLOSE_BUTTON_SIZE)) /
          2;

  gfx::Size alert_indicator_size = alert_indicator_->GetPreferredSize();
  const int alert_indicator_padding =
      (height - alert_indicator_size.height()) / 2;
  const int alert_indicator_right =
      close_button_->GetVisible()
          ? close_button_icon_x - kAfterAlertIndicatorPadding
          : contents_rect.right() - alert_indicator_padding;
  const gfx::Rect alert_indicator_bounds(
      alert_indicator_right - alert_indicator_size.width(),
      contents_rect.y() + alert_indicator_padding, alert_indicator_size.width(),
      alert_indicator_size.height());
  layouts.child_layouts.emplace_back(alert_indicator_.get(),
                                     alert_indicator_->GetVisible(),
                                     alert_indicator_bounds);

  // kAfterIconPadding is the space between the title and the favicon, however
  // icon_ has extra space for the attention indicator and discard ring, given
  // by its insets.
  const int title_bounds_x =
      icon_bounds.right() - icon_->GetInsets().right() + kAfterIconPadding;
  const int title_bounds_y = contents_rect.y() + (height - kTitleHeight) / 2;
  const int title_bounds_right =
      alert_indicator_->GetVisible()
          ? alert_indicator_bounds.x() - kAfterTitlePadding
      : close_button_->GetVisible()
          ? close_button_icon_x - kAfterTitlePadding
          : contents_rect.right() - kTitleNoCloseButtonRightPadding;
  const gfx::Rect title_bounds(title_bounds_x, title_bounds_y,
                               title_bounds_right - title_bounds_x,
                               kTitleHeight);
  layouts.child_layouts.emplace_back(title_.get(), title_->GetVisible(),
                                     title_bounds);
  return layouts;
}

bool VerticalTabView::ShouldEnableMuteToggle(int required_width) {
  // TODO(crbug.com/454686636): Determine if there is enough space to activate
  // the tab in collapsed, pinned, or split states.
  return true;
}

void VerticalTabView::ToggleTabAudioMute() {
  // TODO(crbug.com/462151622): Hook up tab muting through the tab strip model.
  NOTIMPLEMENTED();
}

bool VerticalTabView::IsApparentlyActive() const {
  // TODO(crbug.com/457522224): Use hover state and active/selected state to
  // determine if the tab looks like it is active.
  return true;
}

void VerticalTabView::AlertStateChanged() {
  // TODO(crbug.com/457525548): Update hover card.
  UpdateAlertIndicatorVisibility();
  InvalidateLayout();
}

void VerticalTabView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

void VerticalTabView::OnDataChanged() {
  tabs_api::mojom::Tab tab = *collection_node_->data()->get_tab();
  icon_->SetData(tab);
  title_->SetText(base::UTF8ToUTF16(tab.title));
  alert_indicator_->TransitionToAlertState(
      tab.alert_states.size()
          ? std::make_optional(
                tabs_api::converters::FromMojo(tab.alert_states[0]))
          : std::nullopt);
  UpdateAlertIndicatorVisibility();
  // TODO(crbug.com/457522224): Set visibility based on active and hovered
  // states.
  close_button_->SetVisible(tab.is_active);

  // TODO(crbug.com/460535066): Update tab colors.

  InvalidateLayout();
}

void VerticalTabView::UpdateAlertIndicatorVisibility() {
  alert_indicator_->UpdateAlertIndicatorAnimation();
  alert_indicator_->SetVisible(
      alert_indicator_->showing_alert_state().has_value());
}

BEGIN_METADATA(VerticalTabView)
END_METADATA
