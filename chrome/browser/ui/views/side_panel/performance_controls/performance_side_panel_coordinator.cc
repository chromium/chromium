// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/performance_controls/performance_side_panel_coordinator.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/vector_icons.h"

PerformanceSidePanelCoordinator::PerformanceSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<PerformanceSidePanelCoordinator>(*browser) {}

PerformanceSidePanelCoordinator::~PerformanceSidePanelCoordinator() {
  auto* global_registry =
      SidePanelCoordinator::GetGlobalSidePanelRegistry(&GetBrowser());
  if (global_registry) {
    global_registry->Deregister(
        SidePanelEntry::Key(SidePanelEntry::Id::kPerformance));
  }
}

void PerformanceSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);

  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kPerformance,
      l10n_util::GetStringUTF16(IDS_SHOW_PERFORMANCE),
      ui::ImageModel::FromVectorIcon(kHighEfficiencyIcon, ui::kColorIcon,
                                     icon_size),
      base::BindRepeating(
          &PerformanceSidePanelCoordinator::CreatePerformanceWebUIView,
          base::Unretained(this))));
}

std::unique_ptr<views::View>
PerformanceSidePanelCoordinator::CreatePerformanceWebUIView() {
  auto wrapper =
      std::make_unique<BubbleContentsWrapperT<PerformanceSidePanelUI>>(
          GURL(chrome::kChromeUIPerformanceSidePanelURL),
          GetBrowser().profile(), IDS_SHOW_PERFORMANCE,
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false);
  auto view = std::make_unique<SidePanelWebUIViewT<PerformanceSidePanelUI>>(
      base::RepeatingClosure(), base::RepeatingClosure(), std::move(wrapper));
  view->SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(view.get())->SetAvailable(true);
  return view;
}

BROWSER_USER_DATA_KEY_IMPL(PerformanceSidePanelCoordinator);
