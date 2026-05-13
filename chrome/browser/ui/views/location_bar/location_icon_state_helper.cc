// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_state_helper.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)              // nocheck
#include "chrome/grit/theme_resources.h"
#include "components/vector_icons/vector_icons.h"  // nogncheck
#include "ui/base/resource/resource_bundle.h"
#endif

namespace location_bar {

std::u16string GetSecurityChipText(const LocationBarModel* model,
                                   content::WebContents* web_contents,
                                   bool is_editing_or_empty) {
  if (is_editing_or_empty) {
    return std::u16string();
  }

  if (model->GetURL().SchemeIs(content::kChromeUIScheme) ||
      (contextual_tasks::ShouldShowExpandedSecurityChip() &&
       model->IsContextualTasksPage())) {
    return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  }

  if (model->GetURL().SchemeIs(url::kFileScheme)) {
    return l10n_util::GetStringUTF16(IDS_OMNIBOX_FILE);
  }

  if (model->GetURL().SchemeIs(dom_distiller::kDomDistillerScheme)) {
    return l10n_util::GetStringUTF16(IDS_OMNIBOX_READER_MODE);
  }

  if (web_contents) {
    // On ChromeOS, this can be called using web_contents from
    // SimpleWebViewDialog::GetWebContents() which always returns null.
    // TODO(crbug.com/40501128) Remove the null check and make
    // SimpleWebViewDialog::GetWebContents return the proper web contents
    // instead.
    const std::u16string extension_name =
        extensions::ui_util::GetEnabledExtensionNameForUrl(
            model->GetURL(), web_contents->GetBrowserContext());
    if (!extension_name.empty()) {
      return extension_name;
    }
  }

  return model->GetSecureDisplayText();
}

bool ShouldShowSecurityChipText(const LocationBarModel* model,
                                bool is_editing_or_empty) {
  if (is_editing_or_empty) {
    return false;
  }

  const GURL& url = model->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(url::kFileScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme) ||
      (model->IsContextualTasksPage() &&
       contextual_tasks::ShouldShowExpandedSecurityChip())) {
    return true;
  }

  return !model->GetSecureDisplayText().empty();
}

bool IsGradientGoogleSuperGIcon(const ui::ImageModel& icon) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!icon.IsImage()) {
    return false;
  }
  gfx::ImageSkia image = icon.GetImage().AsImageSkia();
  gfx::ImageSkia target_16 =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GOOGLE_G_GRADIENT_16_ALT);
  gfx::ImageSkia target_20 =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GOOGLE_G_GRADIENT_20);
  return image.BackedBySameObjectAs(target_16) ||
         image.BackedBySameObjectAs(target_20);
#else
  return false;
#endif
}

SecurityChipIcon GetSecurityChipIconEnum(const LocationBarModel* model,
                                         bool is_add_context_button_shown) {
  if (is_add_context_button_shown) {
    return SecurityChipIcon::kAddContext;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)  // nocheck
  const gfx::VectorIcon& icon = model->GetVectorIcon();
  const char* icon_name = icon.name;
  // TODO(b/507061157): Use gradient icon here instead of
  //   `vector_icons::kGoogleSuperGIcon`.
  if (icon_name == vector_icons::kGoogleSuperGIcon.name) {
    return SecurityChipIcon::kGoogleSuperG;
  }
  if (icon_name == vector_icons::kGoogleGLogoMonochromeIcon.name) {
    return SecurityChipIcon::kGoogleGMonochrome;
  }
#endif

  auto security_level = model->GetSecurityLevel();
  if (security_level == security_state::DANGEROUS) {
    return SecurityChipIcon::kDangerous;
  } else if (security_level == security_state::WARNING) {
    return SecurityChipIcon::kNotSecureWarning;
  } else if (security_level == security_state::SECURE) {
    return SecurityChipIcon::kSecurePageInfo;
  }
  return SecurityChipIcon::kHttp;
}

bool IsSecurityChipInteractive(bool is_editing_or_empty,
                               SecurityChipIcon icon) {
  if (is_editing_or_empty) {
    return false;
  }
  if (icon == SecurityChipIcon::kGoogleSuperG ||
      icon == SecurityChipIcon::kGoogleGMonochrome) {
    return false;
  }
  return true;
}

SecurityChipAccessibilityState GetSecurityChipAccessibilityState(
    const LocationBarModel* model,
    bool is_editing_or_empty,
    std::u16string_view current_label) {
  SecurityChipAccessibilityState state;
  state.role = is_editing_or_empty ? ax::mojom::Role::kImage
                                   : ax::mojom::Role::kPopUpButton;

  if (is_editing_or_empty) {
    state.name = l10n_util::GetStringUTF16(IDS_ACC_SEARCH_ICON);
  } else {
    state.name = std::u16string(current_label);
    if (current_label.empty()) {
      // If no display text exists, ensure that the accessibility label is
      // added.
      state.description = model->GetSecureAccessibilityText();
    }
  }

  return state;
}

std::u16string GetSecurityChipTooltipText(bool is_editing_or_empty) {
  if (is_editing_or_empty) {
    return std::u16string();
  }
  return l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON);
}

bool ShouldAnimateSecurityChipTextChange(
    bool is_editing_or_empty,
    security_state::SecurityLevel previous_level,
    security_state::SecurityLevel new_level) {
  if (is_editing_or_empty) {
    return false;
  }
  // Do not animate transitions from WARNING to DANGEROUS, since
  // the transition can look confusing/messy.
  if (new_level == security_state::DANGEROUS &&
      previous_level == security_state::WARNING) {
    return false;
  }
  return new_level == security_state::DANGEROUS ||
         new_level == security_state::WARNING;
}

}  // namespace location_bar
