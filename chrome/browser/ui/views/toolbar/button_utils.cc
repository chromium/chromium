// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/button_utils.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/views/accessibility/view_accessibility.h"

std::unique_ptr<ToolbarButton> CreateBackButton(views::ButtonListener* listener,
                                                Browser* browser) {
  auto back = std::make_unique<ToolbarButton>(
      listener,
      std::make_unique<BackForwardMenuModel>(
          browser, BackForwardMenuModel::ModelType::kBackward),
      browser->tab_strip_model());
  back->set_hide_ink_drop_when_showing_context_menu(false);
  back->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                    ui::EF_MIDDLE_MOUSE_BUTTON);
  back->set_tag(IDC_BACK);
  back->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
  back->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back->GetViewAccessibility().OverrideDescription(
      l10n_util::GetStringUTF8(IDS_ACCDESCRIPTION_BACK));
  back->SetID(VIEW_ID_BACK_BUTTON);
  back->Init();
  return back;
}

std::unique_ptr<ToolbarButton> CreateForwardButton(
    views::ButtonListener* listener,
    Browser* browser) {
  auto forward = std::make_unique<ToolbarButton>(
      listener,
      std::make_unique<BackForwardMenuModel>(
          browser, BackForwardMenuModel::ModelType::kForward),
      browser->tab_strip_model());
  forward->set_hide_ink_drop_when_showing_context_menu(false);
  forward->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_MIDDLE_MOUSE_BUTTON);
  forward->set_tag(IDC_FORWARD);
  forward->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD));
  forward->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));
  forward->GetViewAccessibility().OverrideDescription(
      l10n_util::GetStringUTF8(IDS_ACCDESCRIPTION_FORWARD));
  forward->SetID(VIEW_ID_FORWARD_BUTTON);
  forward->Init();
  return forward;
}

std::unique_ptr<ReloadButton> CreateReloadButton(Browser* browser) {
  auto reload = std::make_unique<ReloadButton>(browser->command_controller());
  reload->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                      ui::EF_MIDDLE_MOUSE_BUTTON);
  reload->set_tag(IDC_RELOAD);
  reload->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  reload->SetID(VIEW_ID_RELOAD_BUTTON);
  reload->Init();
  return reload;
}

std::unique_ptr<HomeButton> CreateHomeButton(views::ButtonListener* listener,
                                             Browser* browser) {
  auto home = std::make_unique<HomeButton>(listener, browser);
  home->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                    ui::EF_MIDDLE_MOUSE_BUTTON);
  home->set_tag(IDC_HOME);
  home->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_HOME));
  home->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_HOME));
  home->SetID(VIEW_ID_HOME_BUTTON);
  home->Init();
  home->SizeToPreferredSize();
  return home;
}
