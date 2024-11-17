// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

#include <utility>

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_loading_indicator_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view_observer.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/platform_style.h"

float PageActionIconView::Delegate::GetPageActionInkDropVisibleOpacity() const {
  return kOmniboxOpacitySelected;
}

int PageActionIconView::Delegate::GetPageActionIconSize() const {
  return GetLayoutConstant(LOCATION_BAR_TRAILING_ICON_SIZE);
}

gfx::Insets PageActionIconView::Delegate::GetPageActionIconInsets(
    const PageActionIconView* icon_view) const {
  return GetLayoutInsets(LOCATION_BAR_PAGE_ACTION_ICON_PADDING);
}

bool PageActionIconView::Delegate::ShouldHidePageActionIcons() const {
  return false;
}

bool PageActionIconView::Delegate::ShouldHidePageActionIcon(
    PageActionIconView* icon_view) const {
  return false;
}

PageActionIconView::PageActionIconView(
    CommandUpdater* command_updater,
    int command_id,
    IconLabelBubbleView::Delegate* parent_delegate,
    PageActionIconView::Delegate* delegate,
    const char* name_for_histograms,
    std::optional<actions::ActionId> action_id,
    Browser* browser,
    bool ephemeral,
    const gfx::FontList& font_list)
    : IconLabelBubbleView(font_list, parent_delegate),
      command_updater_(command_updater),
      delegate_(delegate),
      command_id_(command_id),
      action_id_(action_id),
      name_for_histograms_(name_for_histograms),
      ephemeral_(ephemeral),
      browser_(browser) {
  DCHECK(delegate_);

  image_container_view()->SetFlipCanvasOnPaintForRTLUI(true);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  SetFocusBehavior(views::PlatformStyle::kDefaultFocusBehavior);
  // Only shows bubble after mouse is released.
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnRelease);
  UpdateBorder();
}

PageActionIconView::~PageActionIconView() = default;

void PageActionIconView::AddPageIconViewObserver(
    PageActionIconViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PageActionIconView::RemovePageIconViewObserver(
    PageActionIconViewObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool PageActionIconView::IsBubbleShowing() const {
  // If the bubble is being destroyed, it's considered showing though it may be
  // already invisible currently.
  return GetBubble() != nullptr;
}

bool PageActionIconView::SetCommandEnabled(bool enabled) const {
  DCHECK(command_updater_);
  command_updater_->UpdateCommandEnabled(command_id_, enabled);
  return command_updater_->IsCommandEnabled(command_id_);
}

SkColor PageActionIconView::GetLabelColorForTesting() const {
  return label()->GetEnabledColor();
}

void PageActionIconView::ExecuteForTesting() {
  DCHECK(GetVisible());
  OnExecuting(EXECUTE_SOURCE_MOUSE);
}

void PageActionIconView::InstallLoadingIndicatorForTesting() {
  InstallLoadingIndicator();
}

std::u16string PageActionIconView::GetTextForTooltipAndAccessibleName() const {
  return GetViewAccessibility().GetCachedName();
}

std::u16string PageActionIconView::GetTooltipText(const gfx::Point& p) const {
  return IsBubbleShowing() ? std::u16string()
                           : GetTextForTooltipAndAccessibleName();
}

void PageActionIconView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add && details.child == this) {
    UpdateIconImage();
    UpdateBorder();
  }
}

void PageActionIconView::OnThemeChanged() {
  IconLabelBubbleView::OnThemeChanged();
  UpdateIconImage();
}

bool PageActionIconView::ShouldShowSeparator() const {
  return false;
}

void PageActionIconView::NotifyClick(const ui::Event& event) {
  observer_list_.Notify(
      &PageActionIconViewObserver::OnPageActionIconViewClicked, this);
  // Intentionally skip the immediate parent function
  // IconLabelBubbleView::NotifyClick(). It calls ShowBubble() which
  // is redundant here since we use Chrome command to show the bubble.
  LabelButton::NotifyClick(event);
  ExecuteSource source;
  if (event.IsMouseEvent()) {
    source = EXECUTE_SOURCE_MOUSE;
  } else if (event.IsKeyEvent()) {
    source = EXECUTE_SOURCE_KEYBOARD;
  } else {
    CHECK(event.IsGestureEvent());
    source = EXECUTE_SOURCE_GESTURE;
  }

  // Set ink drop state to ACTIVATED.
  SetHighlighted(true);
  ExecuteCommand(source);
}

bool PageActionIconView::IsTriggerableEvent(const ui::Event& event) {
  // For PageActionIconView, returns whether the bubble should be shown given
  // the event happened. For mouse event, only shows bubble when the bubble is
  // not visible and when event is a left button click.
  if (event.IsMouseEvent()) {
    // IconLabelBubbleView allows any mouse click to be triggerable event so
    // need to manually check here.
    return IconLabelBubbleView::IsTriggerableEvent(event) &&
           ((GetTriggerableEventFlags() & event.flags()) != 0);
  }

  return IconLabelBubbleView::IsTriggerableEvent(event);
}

bool PageActionIconView::ShouldUpdateInkDropOnClickCanceled() const {
  // Override IconLabelBubbleView since for PageActionIconView if click is
  // cancelled due to bubble being visible, the InkDropState is ACTIVATED. So
  // the ink drop will not be updated anyway. Setting this to true will help to
  // update ink drop in other cases where clicks are cancelled.
  return true;
}

void PageActionIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  if (command_updater_) {
    command_updater_->ExecuteCommand(command_id_);
  }
  DidExecute(source);
}

const gfx::VectorIcon& PageActionIconView::GetVectorIconBadge() const {
  return gfx::kNoneIcon;
}

ui::ImageModel PageActionIconView::GetSizedIconImage(int size) const {
  return ui::ImageModel();
}

void PageActionIconView::OnTouchUiChanged() {
  UpdateIconImage();
  IconLabelBubbleView::OnTouchUiChanged();
}

void PageActionIconView::SetIconColor(SkColor icon_color) {
  if (icon_color_ == icon_color) {
    return;
  }
  icon_color_ = icon_color;
  UpdateIconImage();
  OnPropertyChanged(&icon_color_, views::kPropertyEffectsNone);
}

SkColor PageActionIconView::GetIconColor() const {
  return icon_color_;
}

void PageActionIconView::SetActive(bool active) {
  if (active_ == active) {
    return;
  }
  active_ = active;
  UpdateIconImage();
  OnPropertyChanged(&active_, views::kPropertyEffectsNone);
}

bool PageActionIconView::GetActive() const {
  return active_;
}

void PageActionIconView::Update() {
  // Currently no page action icon should be visible during user input.
  // A future subclass may need a hook here if that changes.
  if (delegate_->ShouldHidePageActionIcons()) {
    ResetSlideAnimation(/*show_label=*/false);
    SetVisible(false);
  } else {
    UpdateImpl();
  }
  UpdateBorder();
}

void PageActionIconView::UpdateIconImage() {
  // If PageActionIconView is not hosted within a Widget hierarchy early return
  // here. `UpdateIconImage()` is called in OnThemeChanged() and will update as
  // needed when added to a Widget and on theme changes. Returning early avoids
  // a call to GetNativeTheme() when no hosting Widget is present which falls
  // through to the deprecated global NativeTheme accessor.
  if (!GetWidget()) {
    return;
  }

  // Use the provided icon image if available.
  const int icon_size = delegate_->GetPageActionIconSize();
  const auto icon_image = GetSizedIconImage(icon_size);
  if (!icon_image.IsEmpty()) {
    DCHECK_EQ(icon_image.Size(), gfx::Size(icon_size, icon_size));
    SetImageModel(icon_image);
    return;
  }

  // Fall back to the vector icon if no icon image was provided.
  SkColor icon_color =
      active_ ? views::GetCascadingAccentColor(this) : icon_color_;
  if (IconColorShouldMatchForeground()) {
    icon_color = GetForegroundColor();
  }
  const gfx::ImageSkia image = gfx::CreateVectorIconWithBadge(
      GetVectorIcon(), icon_size, icon_color, GetVectorIconBadge());
  if (!image.isNull()) {
    SetImageModel(ui::ImageModel::FromImageSkia(image));
  }
}

void PageActionIconView::SetIsLoading(bool is_loading) {
  if (loading_indicator_) {
    loading_indicator_->SetAnimating(is_loading);
  }
}

content::WebContents* PageActionIconView::GetWebContents() const {
  return delegate_->GetWebContentsForPageActionIconView();
}

void PageActionIconView::UpdateBorder() {
  gfx::Insets new_insets = delegate_->GetPageActionIconInsets(this);
  if (ShouldShowLabel()) {
    // TODO(crbug.com/40913366): Figure out what these values should be. For
    // bonus point also try to move parts of this into the parent class. This is
    // too bespoke.
    new_insets += gfx::Insets::TLBR(0, 4, 0, 8);
  }
  if (new_insets != GetInsets()) {
    SetBorder(views::CreateEmptyBorder(new_insets));
  }
}

void PageActionIconView::InstallLoadingIndicator() {
  if (loading_indicator_) {
    return;
  }

  loading_indicator_ =
      AddChildView(std::make_unique<PageActionIconLoadingIndicatorView>(this));
  loading_indicator_->SetVisible(false);
}

void PageActionIconView::SetVisible(bool visible) {
  const bool was_visible = GetVisible();
  IconLabelBubbleView::SetVisible(visible);
  if (!was_visible && visible) {
    observer_list_.Notify(
        &PageActionIconViewObserver::OnPageActionIconViewShown, this);
  }
}

BEGIN_METADATA(PageActionIconView)
ADD_PROPERTY_METADATA(SkColor, IconColor, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(bool, Active)
END_METADATA
