// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/app/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#endif

// GlobalError ---------------------------------------------------------------

GlobalError::GlobalError() {}

GlobalError::~GlobalError() {}

GlobalError::Severity GlobalError::GetSeverity() { return SEVERITY_MEDIUM; }

ui::ImageModel GlobalError::MenuItemIcon() {
#if BUILDFLAG(IS_ANDROID)
  return ui::ImageModel(
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_INPUT_ALERT_MENU));
#else
  return ui::ImageModel::FromVectorIcon(kBrowserToolsErrorIcon,
                                        ui::kColorAlertMediumSeverityIcon);
#endif
}

// GlobalErrorWithStandardBubble ---------------------------------------------

GlobalErrorWithStandardBubble::GlobalErrorWithStandardBubble() = default;

GlobalErrorWithStandardBubble::~GlobalErrorWithStandardBubble() = default;

bool GlobalErrorWithStandardBubble::HasBubbleView() { return true; }

bool GlobalErrorWithStandardBubble::HasShownBubbleView() {
  return has_shown_bubble_view_;
}

void GlobalErrorWithStandardBubble::ShowBubbleView(Browser* browser) {
  has_shown_bubble_view_ = true;
  bubble_view_ =
      GlobalErrorBubbleViewBase::ShowStandardBubbleView(browser, AsWeakPtr());
}

GlobalErrorBubbleViewBase* GlobalErrorWithStandardBubble::GetBubbleView() {
  return bubble_view_;
}

bool GlobalErrorWithStandardBubble::ShouldCloseOnDeactivate() const {
  return true;
}

bool GlobalErrorWithStandardBubble::ShouldShowCloseButton() const {
  return false;
}

std::u16string
GlobalErrorWithStandardBubble::GetBubbleViewDetailsButtonLabel() {
  return {};
}

void GlobalErrorWithStandardBubble::BubbleViewDetailsButtonPressed(
    Browser* browser) {}

bool GlobalErrorWithStandardBubble::ShouldAddElevationIconToAcceptButton() {
  return false;
}

int GlobalErrorWithStandardBubble::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_OK;
}

void GlobalErrorWithStandardBubble::BubbleViewDidClose(Browser* browser) {
  DCHECK(browser);
  bubble_view_ = nullptr;
  OnBubbleViewDidClose(browser);
}
