// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "build/build_config.h"
#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/chromeos/devicetype_utils.h"
#endif

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
}

IntentChipButton::~IntentChipButton() = default;

void IntentChipButton::Update() {
  bool was_visible = GetVisible();
  bool is_visible = GetShowChip();
  SetVisible(is_visible);

  if (is_visible) {
    bool expanded = GetChipExpanded();
    ResetAnimation(expanded);
    SetTheme(expanded ? OmniboxChipTheme::kLowVisibility
                      : OmniboxChipTheme::kIconStyle);
    UpdateIconAndColors();
  }
  if (browser_->window()) {
    if (is_visible && !was_visible) {
      // Might want to show the intent chip promo, but can't until the view is
      // properly laid out.
      pending_promo_ = true;
    } else if (was_visible && !is_visible) {
      pending_promo_ = false;
      IntentPickerBubbleView::CloseCurrentBubble();
      browser_->window()->CloseFeaturePromo(
          feature_engagement::kIPHIntentChipFeature);
    }
  }
}

ui::ImageModel IntentChipButton::GetIconImageModel() const {
  auto icon = GetAppIcon();
  if (icon.IsEmpty())
    return OmniboxChipButton::GetIconImageModel();
  return icon;
}

const gfx::VectorIcon& IntentChipButton::GetIcon() const {
  return vector_icons::kOpenInNewIcon;
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
  if (auto* tab_helper = GetTabHelper())
    return tab_helper->app_icon();
  return ui::ImageModel();
}

void IntentChipButton::HandlePressed() {
  browser_->window()->CloseFeaturePromo(
      feature_engagement::kIPHIntentChipFeature);
  content::WebContents* web_contents =
      delegate_->GetWebContentsForPageActionIconView();
  const GURL& url = web_contents->GetURL();
  apps::ShowIntentPickerOrLaunchApp(web_contents, url);
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

void IntentChipButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  OmniboxChipButton::OnBoundsChanged(previous_bounds);

  if (!GetVisible() || size().IsEmpty())
    return;

  if (pending_promo_) {
    user_education::FeaturePromoSpecification::StringReplacements replacements;
#if BUILDFLAG(IS_CHROMEOS)
    replacements.push_back(ui::GetChromeOSDeviceName());
#endif
    browser_->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHIntentChipFeature, replacements);
    // If the FE backend chooses not to show the promo, waiting until the next
    // resize won't change anything.
    pending_promo_ = false;
  }

  // TODO(dfried): If the help bubble has trouble tracking the chip as it
  // animates, a call to HelpBubbleFactoryRegistry::NotifyAnchorBoundsChanged()
  // here while the promo is active should fix the problem, but I'm not going to
  // put that code in unless we determine there's a problem.
}

BEGIN_METADATA(IntentChipButton, OmniboxChipButton)
END_METADATA
