// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/overflow_button.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

OverflowButton::OverflowButton() {
  auto menu_button_controller = std::make_unique<views::MenuButtonController>(
      this,
      base::BindRepeating(&OverflowButton::RunMenu, base::Unretained(this)),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(this));
  menu_button_controller_ = menu_button_controller.get();
  SetProperty(views::kElementIdentifierKey, kToolbarOverflowButtonElementId);
  SetButtonController(std::move(menu_button_controller));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_OVERFLOW_BUTTON));
  SetVectorIcons(kOverflowButtonIcon, kOverflowButtonTouchIcon);
}

void OverflowButton::RunMenu() {
  CHECK(GetVisible());
  toolbar_controller_->ShowMenu();
  base::RecordAction(
      base::UserMetricsAction("ResponsiveToolbar.OverflowButtonActivated"));
}

OverflowButton::~OverflowButton() = default;

BEGIN_METADATA(OverflowButton)
END_METADATA
