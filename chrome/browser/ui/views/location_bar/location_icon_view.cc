// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/dom_distiller/core/url_constants.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

using content::WebContents;
using security_state::SecurityLevel;

LocationIconView::LocationIconView(
    const gfx::FontList& font_list,
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate)
    : IconLabelBubbleView(font_list, parent_delegate), delegate_(delegate) {
  DCHECK(delegate_);

  SetID(VIEW_ID_LOCATION_ICON);
  SetUpForAnimation();
  SetProperty(views::kElementIdentifierKey, kLocationIconElementId);

  // Readability is guaranteed by the omnibox theme.
  label()->SetAutoColorReadabilityEnabled(false);
}

LocationIconView::~LocationIconView() {}

gfx::Size LocationIconView::GetMinimumSize() const {
  return GetMinimumSizeForPreferredSize(GetPreferredSize());
}

bool LocationIconView::OnMouseDragged(const ui::MouseEvent& event) {
  delegate_->OnLocationIconDragged(event);
  return IconLabelBubbleView::OnMouseDragged(event);
}

SkColor LocationIconView::GetForegroundColor() const {
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

bool LocationIconView::IsBubbleShowing() const {
  return PageInfoBubbleView::GetShownBubbleType() !=
         PageInfoBubbleView::BUBBLE_NONE;
}

bool LocationIconView::OnMousePressed(const ui::MouseEvent& event) {
  delegate_->OnLocationIconPressed(event);

  IconLabelBubbleView::OnMousePressed(event);
  return true;
}

void LocationIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (delegate_->IsEditingOrEmpty()) {
    node_data->role = ax::mojom::Role::kImage;
    node_data->SetNameChecked(l10n_util::GetStringUTF8(IDS_ACC_SEARCH_ICON));
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

void LocationIconView::AddedToWidget() {
  Update(true);
}

void LocationIconView::OnThemeChanged() {
  IconLabelBubbleView::OnThemeChanged();
  UpdateIcon();
}

int LocationIconView::GetMinimumLabelTextWidth() const {
  int width = 0;

  std::u16string text = GetText();
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

bool LocationIconView::GetShowText() const {
  if (delegate_->IsEditingOrEmpty())
    return false;

  const auto* location_bar_model = delegate_->GetLocationBarModel();
  const GURL& url = location_bar_model->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(url::kFileScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme)) {
    return true;
  }

  return !location_bar_model->GetSecureDisplayText().empty();
}

const views::InkDrop* LocationIconView::get_ink_drop_for_testing() {
  return views::InkDrop::Get(this)->GetInkDrop();
}

std::u16string LocationIconView::GetText() const {
  if (delegate_->IsEditingOrEmpty())
    return std::u16string();

  if (delegate_->GetLocationBarModel()->GetURL().SchemeIs(
          content::kChromeUIScheme))
    return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);

  if (delegate_->GetLocationBarModel()->GetURL().SchemeIs(url::kFileScheme))
    return l10n_util::GetStringUTF16(IDS_OMNIBOX_FILE);

  if (delegate_->GetLocationBarModel()->GetURL().SchemeIs(
          dom_distiller::kDomDistillerScheme)) {
    return l10n_util::GetStringUTF16(IDS_OMNIBOX_READER_MODE);
  }

  if (delegate_->GetWebContents()) {
    // On ChromeOS, this can be called using web_contents from
    // SimpleWebViewDialog::GetWebContents() which always returns null.
    // TODO(crbug.com/680329) Remove the null check and make
    // SimpleWebViewDialog::GetWebContents return the proper web contents
    // instead.
    const std::u16string extension_name =
        extensions::ui_util::GetEnabledExtensionNameForUrl(
            delegate_->GetLocationBarModel()->GetURL(),
            delegate_->GetWebContents()->GetBrowserContext());
    if (!extension_name.empty())
      return extension_name;
  }

  return delegate_->GetLocationBarModel()->GetSecureDisplayText();
}

bool LocationIconView::GetAnimateTextVisibilityChange() const {
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

  bool should_show = GetShowText();
  if (!GetAnimateTextVisibilityChange() || suppress_animations)
    ResetSlideAnimation(should_show);
  else if (should_show)
    AnimateIn(absl::nullopt);
  else
    AnimateOut();
}

void LocationIconView::UpdateIcon() {
  // Cancel any previous outstanding icon requests, as they are now outdated.
  icon_fetch_weak_ptr_factory_.InvalidateWeakPtrs();

  ui::ImageModel icon = delegate_->GetLocationIcon(
      base::BindOnce(&LocationIconView::OnIconFetched,
                     icon_fetch_weak_ptr_factory_.GetWeakPtr()));
  if (!icon.IsEmpty())
    SetImageModel(icon);
}

void LocationIconView::OnIconFetched(const gfx::Image& image) {
  DCHECK(!image.IsEmpty());
  SetImageModel(ui::ImageModel::FromImage(image));
}

void LocationIconView::Update(bool suppress_animations) {
  UpdateTextVisibility(suppress_animations);
  UpdateIcon();

  // The label text color may have changed in response to changes in security
  // level.
  UpdateLabelColors();

  bool is_editing_or_empty = delegate_->IsEditingOrEmpty();
  // The tooltip should be shown if we are not editing or empty.
  SetTooltipText(is_editing_or_empty
                     ? std::u16string()
                     : l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));

  // We should only enable/disable the InkDrop if the editing state has changed,
  // as the drop gets recreated when views::InkDrop::Get(this)->SetMode() is
  // called. This can result in strange behaviour, like the the InkDrop
  // disappearing mid animation.
  if (is_editing_or_empty != was_editing_or_empty_) {
    // If the omnibox is empty or editing, the user should not be able to left
    // click on the icon. As such, the icon should not show a highlight or be
    // focusable. Note: using the middle mouse to copy-and-paste should still
    // work on the icon.
    if (is_editing_or_empty) {
      views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
      SetFocusBehavior(FocusBehavior::NEVER);
    } else {
      views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
      SetFocusBehavior(views::PlatformStyle::kDefaultFocusBehavior);
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

BEGIN_METADATA(LocationIconView, IconLabelBubbleView)
ADD_READONLY_PROPERTY_METADATA(int, MinimumLabelTextWidth)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Text)
ADD_READONLY_PROPERTY_METADATA(bool, ShowText)
ADD_READONLY_PROPERTY_METADATA(bool, AnimateTextVisibilityChange)
END_METADATA
