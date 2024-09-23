// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

IntentChipButton::IntentChipButton(Browser* browser,
                                   PageActionIconView::Delegate* delegate)
    : OmniboxChipButton(base::BindRepeating(&IntentChipButton::HandlePressed,
                                            base::Unretained(this))),
      browser_(browser),
      delegate_(delegate) {
  DCHECK(browser);
  SetText(l10n_util::GetStringUTF16(IDS_INTENT_CHIP_OPEN_IN_APP));
  SetFocusBehavior(views::PlatformStyle::kDefaultFocusBehavior);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_INTENT_CHIP_OPEN_IN_APP));
  SetProperty(views::kElementIdentifierKey, kIntentChipElementId);
    label()->SetTextStyle(views::style::STYLE_BODY_3_EMPHASIS);
}

IntentChipButton::~IntentChipButton() = default;

void IntentChipButton::Update() {
  bool was_visible = GetVisible();
  bool is_visible = GetShowChip();
  SetVisible(is_visible);

  if (is_visible) {
    bool expanded = GetChipExpanded();
    SetTheme(expanded ? OmniboxChipTheme::kLowVisibility
                      : OmniboxChipTheme::kIconStyle);
    // TODO(pkasting): This should animate in the way other OmniboxChipButtons
    // are instructed to do (e.g. with non-zero duration when first expanding),
    // but simply passing a non-zero duration here would animate even when
    // that's undesirable (e.g. when switching tabs). In the meantime, we can't
    // simply use ResetAnimation() here because that won't trigger
    // end-of-animation updates like PreferredSizeChanged(). This should be
    // refactored to use common logic with other chips. Note that if this begins
    // to animate in some cases, the browsertests will likely need updates to
    // disable those animations.
    static constexpr auto kAnimationDuration = base::TimeDelta();
    if (expanded) {
      AnimateExpand(kAnimationDuration);
    } else {
      AnimateCollapse(kAnimationDuration);
    }
  }
  if (browser_->window() && was_visible && !is_visible) {
    IntentPickerBubbleView::CloseCurrentBubble();
  }
}

ui::ImageModel IntentChipButton::GetAppIconForTesting() const {
  CHECK_IS_TEST();
  return GetAppIcon();
}

bool IntentChipButton::GetShowChip() const {
  if (delegate_->ShouldHidePageActionIcons())
    return false;

  auto* tab_helper = GetTabHelper();
  return tab_helper && tab_helper->should_show_icon();
}

bool IntentChipButton::GetChipExpanded() const {
  auto* tab_helper = GetTabHelper();
  return tab_helper && tab_helper->ShouldShowExpandedChip();
}

ui::ImageModel IntentChipButton::GetAppIcon() const {
  if (auto* tab_helper = GetTabHelper()) {
    return tab_helper->app_icon();
  }
  return ui::ImageModel();
}

void IntentChipButton::HandlePressed() {
  content::WebContents* web_contents =
      delegate_->GetWebContentsForPageActionIconView();
  const GURL& url = web_contents->GetURL();
  GetTabHelper()->ShowIntentPickerBubbleOrLaunchApp(url);
}

IntentPickerTabHelper* IntentChipButton::GetTabHelper() const {
  if (browser_->profile()->IsOffTheRecord())
    return nullptr;

  content::WebContents* web_contents =
      delegate_->GetWebContentsForPageActionIconView();
  if (!web_contents)
    return nullptr;

  return IntentPickerTabHelper::FromWebContents(web_contents);
}

ui::ImageModel IntentChipButton::GetIconImageModel() const {
  auto icon = GetAppIcon();
  if (icon.IsEmpty()) {
    return OmniboxChipButton::GetIconImageModel();
  }
  return icon;
}

const gfx::VectorIcon& IntentChipButton::GetIcon() const {
    return kOpenInNewChromeRefreshIcon;
}

ui::ColorId IntentChipButton::GetBackgroundColorId() const {
  DCHECK(GetOmniboxChipTheme() != OmniboxChipTheme::kIconStyle);
  return kColorOmniboxIntentChipBackground;
}

ui::ColorId IntentChipButton::GetForegroundColorId() const {
  return GetOmniboxChipTheme() == OmniboxChipTheme::kIconStyle
             ? kColorOmniboxResultsIcon
             : kColorOmniboxIntentChipIcon;
}

BEGIN_METADATA(IntentChipButton)
END_METADATA
