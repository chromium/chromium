// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_coordinator.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_metrics.h"
#include "chrome/browser/ui/webui/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"

using SidePanelWebUIViewT_TabsFromOtherDevicesSidePanelUI =
    SidePanelWebUIViewT<TabsFromOtherDevicesSidePanelUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_TabsFromOtherDevicesSidePanelUI,
                        SidePanelWebUIViewT)
END_METADATA

namespace {
std::unique_ptr<views::View> CreateTabsFromOtherDevicesWebView(
    BrowserWindowInterface* browser,
    Profile* profile,
    base::WeakPtr<TabsFromOtherDevicesSidePanelMetrics> metrics_recorder,
    SidePanelEntryScope& scope) {
  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<TabsFromOtherDevicesSidePanelUI>>(
          GURL(chrome::kChromeUITabsFromOtherDevicesSidePanelURL), profile,
          IDS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TITLE,
          /*esc_closes_ui=*/false,
          /*supports_draggable_regions=*/false);
  contents_wrapper->GetWebUIController()->SetBrowserWindowInterface(browser);
  contents_wrapper->GetWebUIController()->SetMetricsRecorder(metrics_recorder);

  return std::make_unique<SidePanelWebUIViewT<TabsFromOtherDevicesSidePanelUI>>(
      scope, base::RepeatingClosure(), base::RepeatingClosure(),
      std::move(contents_wrapper));
}
}  // namespace

TabsFromOtherDevicesSidePanelCoordinator::
    TabsFromOtherDevicesSidePanelCoordinator(BrowserWindowInterface* browser,
                                             Profile* profile)
    : browser_(CHECK_DEREF(browser)), profile_(CHECK_DEREF(profile)) {}

TabsFromOtherDevicesSidePanelCoordinator::
    ~TabsFromOtherDevicesSidePanelCoordinator() = default;

// static
bool TabsFromOtherDevicesSidePanelCoordinator::IsSupported(Profile* profile) {
  // This side panel is not supported in incognito, guest, etc.
  return profile->IsRegularProfile() &&
         base::FeatureList::IsEnabled(features::kTabsFromOtherDevicesSidePanel);
}

void TabsFromOtherDevicesSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  metrics_recorder_ = std::make_unique<TabsFromOtherDevicesSidePanelMetrics>();

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kTabsFromOtherDevices),
      base::BindRepeating(&CreateTabsFromOtherDevicesWebView, &browser_.get(),
                          &profile_.get(), metrics_recorder_->GetWeakPtr()),
      /*default_content_width_callback=*/base::NullCallback());

  metrics_recorder_->Observe(entry.get());

  global_registry->Register(std::move(entry));
}
