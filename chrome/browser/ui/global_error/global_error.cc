// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"

// GlobalError ---------------------------------------------------------------

GlobalError::GlobalError() = default;

GlobalError::~GlobalError() = default;

GlobalError::Severity GlobalError::GetSeverity() {
  return SEVERITY_MEDIUM;
}

ui::ImageModel GlobalError::MenuItemIcon() {
  return ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                            ? vector_icons::kErrorFilledIcon
                                            : kBrowserToolsErrorOldIcon,
                                        ui::kColorAlertMediumSeverityIcon);
}

// GlobalErrorWithStandardBubble ---------------------------------------------

GlobalErrorWithStandardBubble::GlobalErrorWithStandardBubble() = default;

GlobalErrorWithStandardBubble::~GlobalErrorWithStandardBubble() = default;

bool GlobalErrorWithStandardBubble::HasBubbleView() {
  return true;
}

bool GlobalErrorWithStandardBubble::HasShownBubbleView() {
  return has_shown_bubble_view_;
}

void GlobalErrorWithStandardBubble::ShowBubbleView(
    BrowserWindowInterface* browser) {
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
    BrowserWindowInterface* browser) {}

bool GlobalErrorWithStandardBubble::ShouldAddElevationIconToAcceptButton() {
  return false;
}

void GlobalErrorWithStandardBubble::BubbleViewDidClose(
    BrowserWindowInterface* browser) {
  bubble_view_ = nullptr;
  OnBubbleViewDidClose(browser);
}
