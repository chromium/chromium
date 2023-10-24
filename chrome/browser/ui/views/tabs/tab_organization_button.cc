// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_organization_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTabOrganizeCornerRadius = 10;
}

TabOrganizationButton::TabOrganizationButton(TabStrip* tab_strip,
                                             PressedCallback pressed_callback,
                                             Edge flat_edge)
    : TabStripControlButton(
          tab_strip,
          base::BindRepeating(&TabOrganizationButton::ButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_TAB_ORGANIZE),
          flat_edge),
      pressed_callback_(std::move(pressed_callback)) {
  SetProperty(views::kElementIdentifierKey, kTabOrganizationButtonElementId);

  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE));
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_ORGANIZE));

  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  set_paint_transparent_for_custom_image_theme(false);

  UpdateColors();
}

TabOrganizationButton::~TabOrganizationButton() = default;

void TabOrganizationButton::SetWidthFactor(float factor) {
  width_factor_ = factor;
  PreferredSizeChanged();
}

gfx::Size TabOrganizationButton::CalculatePreferredSize() const {
  const int insets_width = 12;
  const int full_width =
      LabelButton::CalculatePreferredSize().width() + insets_width;
  const int width = full_width * width_factor_;
  const int height = TabStripControlButton::CalculatePreferredSize().height();
  return gfx::Size(width, height);
}

void TabOrganizationButton::ButtonPressed(const ui::Event& event) {
  CHECK(session_);
  if (session_->request()->state() ==
      TabOrganizationRequest::State::NOT_STARTED) {
    session_->StartRequest();
  }
  pressed_callback_.Run(event);
}

int TabOrganizationButton::GetCornerRadius() const {
  return kTabOrganizeCornerRadius;
}

BEGIN_METADATA(TabOrganizationButton, TabStripControlButton)
END_METADATA
