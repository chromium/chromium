// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"

#include "base/check.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

// static
OrganizerPanelHost* OrganizerPanelHost::FromView(views::View* view) {
  CHECK(view);
  if (auto* instance = views::AsViewClass<OrganizerPanelHostView>(view)) {
    return instance;
  }
  if (auto* tray = views::AsViewClass<OrganizerTrayView>(view)) {
    return tray;
  }
  if (view->parent()) {
    return FromView(view->parent());
  }
  return nullptr;
}

// static
OrganizerPanelHost* OrganizerPanelHost::GetPreferredHost(
    BrowserWindowInterface& browser) {
  ui::ElementIdentifier id = OrganizerTrayView::kTrayElementId;
  if (browser.GetType() == BrowserWindowInterface::TYPE_NORMAL) {
    if (auto* const controller =
            tabs::VerticalTabStripStateController::From(&browser)) {
      if (controller->ShouldDisplayVerticalTabs()) {
        // TODO(https://crbug.com/555248711): implement vertical tab strip
        // region as a panel host.
      }
    }
  }
  return FromView(BrowserElementsViews::From(&browser)->GetView(id));
}

OrganizerPanelHostView::OrganizerPanelHostView() = default;
OrganizerPanelHostView::~OrganizerPanelHostView() = default;

BEGIN_METADATA(OrganizerPanelHostView)
END_METADATA
