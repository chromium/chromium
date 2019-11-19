// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "base/bind.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

using content::WebContents;
using security_state::SecurityLevel;

LocationIconView::LocationIconView(const gfx::FontList& font_list,
                                   Delegate* delegate)
    : IconLabelBubbleView(font_list), delegate_(delegate) {
  SetID(VIEW_ID_LOCATION_ICON);
  Update(true);
  SetUpForAnimation();

  // Readability is guaranteed by the omnibox theme.
  label()->SetAutoColorReadabilityEnabled(false);
}

LocationIconView::~LocationIconView() {}

gfx::Size LocationIconView::GetMinimumSize() const {
  return GetMinimumSizeForPreferredSize(GetPreferredSize());
}

bool LocationIconView::OnMousePressed(const ui::MouseEvent& event) {
  delegate_->OnLocationIconPressed(event);

  IconLabelBubbleView::OnMousePressed(event);
  return true;
}

bool LocationIconView::OnMouseDragged(const ui::MouseEvent& event) {
  delegate_->OnLocationIconDragged(event);
  return IconLabelBubbleView::OnMouseDragged(event);
}

SkColor LocationIconView::GetTextColor() const {
  SecurityLevel security_level = SecurityLevel::NONE;
  if (!delegate_->IsEditingOrEmpty())
    security_level = delegate_->GetLocationBarModel()->GetSecurityLevel();

  return delegate_->GetSecurityChipColor(security_level);
}

bool LocationIconView::ShouldShowSeparator() const {
  return ShouldShowLabel();
}

bool LocationIconView::ShowBubble(const ui::Event& event) {
  return delegate_->ShowPageInfoDialog();
}

SkColor LocationIconView::GetInkDropBaseColor() const {
  return delegate_->GetLocationIconInkDropColor();
}

void LocationIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (delegate_->IsEditingOrEmpty()) {
    node_data->role = ax::mojom::Role::kImage;
    node_data->SetName(l10n_util::GetStringUTF8(IDS_ACC_SEARCH_ICON));
    return;
  }

  // If no display text exists, ensure that the accessibility label is added.
  auto accessibility_label = base::UTF16ToUTF8(
      delegate_->GetLocationBarModel()->GetSecureAccessibilityText());
  if (label()->GetText().empty() && !accessibility_label.empty()) {
    node_data->AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                  accessibility_label);
  }

  IconLabelBubbleView::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kPopUpButton;
}

bool LocationIconView::IsBubbleShowing() const {
  return PageInfoBubbleView::GetShownBubbleType() !=
         PageInfoBubbleView::BUBBLE_NONE;
}

int LocationIconView::GetMinimumLabelTextWidth() const {
  int width = 0;

  base::string16 text = GetText();
  if (text == label()->GetText()) {
    // Optimize this common case by not creating a new label.
    // GetPreferredSize is not dependent on the label's current
    // width, so this returns the same value as the branch below.
    width = label()->GetPreferredSize().width();
  } else {
    views::Label label(text, {font_list()});
    width = label.GetPreferredSize().width();
  }
  return GetMinimumSizeForPreferredSize(GetSizeForLabelWidth(width)).width();
}

bool LocationIconView::ShouldShowText() const {
  if (delegate_->IsEditingOrEmpty())
    return false;

  const auto* location_bar_model = delegate_->GetLocationBarModel();
  const GURL& url = location_bar_model->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(url::kFileScheme)) {
    return true;
  }

  return !location_bar_model->GetSecureDisplayText().empty();
}

const views::InkDrop* LocationIconView::get_ink_drop_for_testing() {
  return GetInkDrop();
}

base::string16 LocationIconView::GetText() const {
  if (delegate_->IsEditingOrEmpty())
    return base::string16();

  if (delegate_->GetLocationBarModel()->GetURL().SchemeIs(
          content::kChromeUIScheme))
    return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);

  if (delegate_->GetLocationBarModel()->GetURL().SchemeIs(url::kFileScheme))
    return l10n_util::GetStringUTF16(IDS_OMNIBOX_FILE);

  if (delegate_->GetWebContents()) {
    // On ChromeOS, this can be called using web_contents from
    // SimpleWebViewDialog::GetWebContents() which always returns null.
    // TODO(crbug.com/680329) Remove the null check and make
    // SimpleWebViewDialog::GetWebContents return the proper web contents
    // instead.
    const base::string16 extension_name =
        extensions::ui_util::GetEnabledExtensionNameForUrl(
            delegate_->GetLocationBarModel()->GetURL(),
            delegate_->GetWebContents()->GetBrowserContext());
    if (!extension_name.empty())
      return extension_name;
  }

  return delegate_->GetLocationBarModel()->GetSecureDisplayText();
}

bool LocationIconView::ShouldAnimateTextVisibilityChange() const {
  if (delegate_->IsEditingOrEmpty())
    return false;

  SecurityLevel level = delegate_->GetLocationBarModel()->GetSecurityLevel();
  // Do not animate transitions from WARNING to DANGEROUS, since
  // the transition can look confusing/messy.
  if (level == SecurityLevel::DANGEROUS &&
      last_update_security_level_ == SecurityLevel::WARNING)
    return false;
  return (level == SecurityLevel::DANGEROUS || level == SecurityLevel::WARNING);
}

void LocationIconView::UpdateTextVisibility(bool suppress_animations) {
  SetLabel(GetText());

  bool should_show = ShouldShowText();
  if (!ShouldAnimateTextVisibilityChange() || suppress_animations)
    ResetSlideAnimation(should_show);
  else if (should_show)
    AnimateIn(base::nullopt);
  else
    AnimateOut();

  // The label text color may have changed.
  OnThemeChanged();
}

void LocationIconView::UpdateIcon() {
  // Cancel any previous outstanding icon requests, as they are now outdated.
  icon_fetch_weak_ptr_factory_.InvalidateWeakPtrs();

  gfx::ImageSkia icon = delegate_->GetLocationIcon(
      base::BindOnce(&LocationIconView::OnIconFetched,
                     icon_fetch_weak_ptr_factory_.GetWeakPtr()));

  SetImage(icon);
}

void LocationIconView::OnIconFetched(const gfx::Image& image) {
  SetImage(image.AsImageSkia());
}

void LocationIconView::Update(bool suppress_animations) {
  UpdateTextVisibility(suppress_animations);
  UpdateIcon();

  bool is_editing_or_empty = delegate_->IsEditingOrEmpty();
  // The tooltip should be shown if we are not editing or empty.
  SetTooltipText(is_editing_or_empty
                     ? base::string16()
                     : l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));

  // We should only enable/disable the InkDrop if the editing state has changed,
  // as the drop gets recreated when SetInkDropMode is called. This can result
  // in strange behaviour, like the the InkDrop disappearing mid animation.
  if (is_editing_or_empty != was_editing_or_empty_) {
    // If the omnibox is empty or editing, the user should not be able to left
    // click on the icon. As such, the icon should not show a highlight or be
    // focusable. Note: using the middle mouse to copy-and-paste should still
    // work on the icon.
    if (is_editing_or_empty) {
      SetInkDropMode(InkDropMode::OFF);
      SetFocusBehavior(FocusBehavior::NEVER);
    } else {
      SetInkDropMode(InkDropMode::ON);

#if defined(OS_MACOSX)
      SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
      SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
    }
  }

  last_update_security_level_ = SecurityLevel::NONE;
  if (!is_editing_or_empty) {
    last_update_security_level_ =
        delegate_->GetLocationBarModel()->GetSecurityLevel();
  }
  was_editing_or_empty_ = is_editing_or_empty;
}

bool LocationIconView::IsTriggerableEvent(const ui::Event& event) {
  if (delegate_->IsEditingOrEmpty())
    return false;

  if (event.IsMouseEvent()) {
    if (event.AsMouseEvent()->IsOnlyMiddleMouseButton())
      return false;
  } else if (event.IsGestureEvent() && event.type() != ui::ET_GESTURE_TAP) {
    return false;
  }

  return IconLabelBubbleView::IsTriggerableEvent(event);
}

gfx::Size LocationIconView::GetMinimumSizeForPreferredSize(
    gfx::Size size) const {
  const int kMinCharacters = 10;
  size.SetToMin(
      GetSizeForLabelWidth(font_list().GetExpectedTextWidth(kMinCharacters)));
  return size;
}
