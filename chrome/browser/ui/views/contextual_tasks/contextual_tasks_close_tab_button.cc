// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_tab_button.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_button_controller.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksCloseTabButton,
                                      kContextualTasksCloseTabButton);

ContextualTasksCloseTabButton::ContextualTasksCloseTabButton(
    BrowserWindowInterface* browser_window_interface)
    : ToolbarButton(
          base::BindRepeating(&ContextualTasksCloseTabButton::OnButtonPress,
                              base::Unretained(this)),
          nullptr,
          nullptr),
      browser_window_interface_(browser_window_interface) {
  SetProperty(views::kElementIdentifierKey, kContextualTasksCloseTabButton);
  const std::u16string button_tooltip = l10n_util::GetStringUTF16(IDS_CLOSE);
  GetViewAccessibility().SetName(button_tooltip);
  SetTooltipText(button_tooltip);
  SetVectorIcon(vector_icons::kCloseIcon);

  ContextualTasksCloseButtonController* const controller =
      ContextualTasksCloseButtonController::From(browser_window_interface_);
  CHECK(controller);
  should_update_visibility_subscription_ =
      controller->RegisterShouldUpdateButtonVisibility(base::BindRepeating(
          &ContextualTasksCloseTabButton::OnShouldUpdateVisibility,
          base::Unretained(this)));
  controller->MaybeNotifyVisibilityShouldChange();
}

ContextualTasksCloseTabButton::~ContextualTasksCloseTabButton() = default;

void ContextualTasksCloseTabButton::OnButtonPress() {
  ContextualTasksCloseButtonController* const controller =
      ContextualTasksCloseButtonController::From(browser_window_interface_);
  CHECK(controller);
  controller->MaybeCloseTabExpandSidePanel();
}

void ContextualTasksCloseTabButton::OnShouldUpdateVisibility(bool should_show) {
  SetVisible(should_show);
}

BEGIN_METADATA(ContextualTasksCloseTabButton)
END_METADATA
