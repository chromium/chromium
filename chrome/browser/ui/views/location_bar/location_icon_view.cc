// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/toolbar_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

using content::WebContents;

LocationIconView::LocationIconView(const gfx::FontList& font_list,
                                   LocationBarView* location_bar)
    : IconLabelBubbleView(font_list), location_bar_(location_bar) {
  label()->SetElideBehavior(gfx::ELIDE_MIDDLE);
  set_id(VIEW_ID_LOCATION_ICON);
  Update();
  SetUpForInOutAnimation();
}

LocationIconView::~LocationIconView() {
}

gfx::Size LocationIconView::GetMinimumSize() const {
  return GetMinimumSizeForPreferredSize(GetPreferredSize());
}

bool LocationIconView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton() &&
      ui::Clipboard::IsSupportedClipboardType(ui::CLIPBOARD_TYPE_SELECTION)) {
    base::string16 text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::CLIPBOARD_TYPE_SELECTION, &text);
    text = OmniboxView::SanitizeTextForPaste(text);
    OmniboxEditModel* model = location_bar_->GetOmniboxView()->model();
    if (model->CanPasteAndGo(text))
      model->PasteAndGo(text, event.time_stamp());
  }

  IconLabelBubbleView::OnMousePressed(event);
  return true;
}

bool LocationIconView::OnMouseDragged(const ui::MouseEvent& event) {
  location_bar_->GetOmniboxView()->CloseOmniboxPopup();
  return IconLabelBubbleView::OnMouseDragged(event);
}

bool LocationIconView::GetTooltipText(const gfx::Point& p,
                                      base::string16* tooltip) const {
  if (show_tooltip_)
    *tooltip = l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON);
  return show_tooltip_;
}

SkColor LocationIconView::GetTextColor() const {
  return location_bar_->GetSecurityChipColor(
      location_bar_->GetToolbarModel()->GetSecurityLevel(false));
}

bool LocationIconView::ShouldShowSeparator() const {
  if (ShouldShowLabel())
    return true;

  if (OmniboxFieldTrial::IsJogTextfieldOnPopupEnabled())
    return false;

  return !location_bar_->GetOmniboxView()->IsEditingOrEmpty();
}

bool LocationIconView::ShouldShowExtraEndSpace() const {
  if (OmniboxFieldTrial::IsJogTextfieldOnPopupEnabled())
    return false;

  return location_bar_->GetOmniboxView()->IsEditingOrEmpty();
}

bool LocationIconView::ShowBubble(const ui::Event& event) {
  auto* contents = location_bar_->GetWebContents();
  if (!contents)
    return false;
  return location_bar_->ShowPageInfoDialog(contents);
}

SkColor LocationIconView::GetInkDropBaseColor() const {
  return location_bar_->GetIconInkDropColor();
}

void LocationIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (location_bar_->GetOmniboxView()->IsEditingOrEmpty()) {
    node_data->role = ax::mojom::Role::kImage;
    node_data->SetName(l10n_util::GetStringUTF8(IDS_ACC_SEARCH_ICON));
    return;
  }

  security_state::SecurityLevel security_level =
      location_bar_->GetToolbarModel()->GetSecurityLevel(false);
  if (label()->text().empty() && (security_level == security_state::EV_SECURE ||
                                  security_level == security_state::SECURE)) {
    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kDescription,
        l10n_util::GetStringUTF8(IDS_SECURE_VERBOSE_STATE));
  }

  IconLabelBubbleView::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kPopUpButton;
}

bool LocationIconView::IsBubbleShowing() const {
  return PageInfoBubbleView::GetShownBubbleType() !=
         PageInfoBubbleView::BUBBLE_NONE;
}

gfx::Size LocationIconView::GetMinimumSizeForLabelText(
    const base::string16& text) const {
  int width = 0;
  if (text == label()->text()) {
    // Optimize this common case by not creating a new label.
    // GetPreferredSize is not dependent on the label's current
    // width, so this returns the same value as the branch below.
    width = label()->GetPreferredSize().width();
  } else {
    views::Label label(text, {font_list()});
    width = label.GetPreferredSize().width();
  }
  return GetMinimumSizeForPreferredSize(GetSizeForLabelWidth(width));
}

void LocationIconView::SetTextVisibility(bool should_show,
                                         bool should_animate) {
  if (!should_animate)
    ResetSlideAnimation(should_show);
  else if (should_show)
    AnimateIn(base::nullopt);
  else
    AnimateOut();

  // The label text color may have changed.
  OnNativeThemeChanged(GetNativeTheme());
}

void LocationIconView::Update() {
  // If the omnibox is empty or editing, the user should not be able to left
  // click on the icon. As such, the icon should not show a highlight or be
  // focusable. Note: using the middle mouse to copy-and-paste should still
  // work on the icon.
  if (location_bar_->GetOmniboxView() &&
      location_bar_->GetOmniboxView()->IsEditingOrEmpty()) {
    SetInkDropMode(InkDropMode::OFF);
    SetFocusBehavior(FocusBehavior::NEVER);
    return;
  }

  SetInkDropMode(InkDropMode::ON);

#if defined(OS_MACOSX)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
}

bool LocationIconView::IsTriggerableEvent(const ui::Event& event) {
  if (location_bar_->GetOmniboxView()->IsEditingOrEmpty())
    return false;

  if (event.IsMouseEvent()) {
    if (event.AsMouseEvent()->IsOnlyMiddleMouseButton())
      return false;
  } else if (event.IsGestureEvent() && event.type() != ui::ET_GESTURE_TAP) {
    return false;
  }

  return IconLabelBubbleView::IsTriggerableEvent(event);
}

double LocationIconView::WidthMultiplier() const {
  return GetAnimationValue();
}

gfx::Size LocationIconView::GetMinimumSizeForPreferredSize(
    gfx::Size size) const {
  const int kMinCharacters = 10;
  size.SetToMin(
      GetSizeForLabelWidth(font_list().GetExpectedTextWidth(kMinCharacters)));
  return size;
}

int LocationIconView::GetSlideDurationTime() const {
  constexpr int kSlideDurationTimeMs = 150;
  return kSlideDurationTimeMs;
}
