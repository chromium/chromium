// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/performance_controls/performance_side_panel_coordinator.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_panel/performance_controls/performance_side_panel_model.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_model_host.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance.mojom-shared-internal.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance.mojom-shared.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_PerformanceSidePanelUI =
    SidePanelWebUIViewT<PerformanceSidePanelUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_PerformanceSidePanelUI,
                        SidePanelWebUIViewT)
END_METADATA

PerformanceSidePanelCoordinator::PerformanceSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<PerformanceSidePanelCoordinator>(*browser) {
  performance_state_observer_ =
      std::make_unique<PerformanceStateObserver>(browser);
}

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
      ui::ImageModel::FromVectorIcon(kMemorySaverIcon, ui::kColorIcon,
                                     icon_size),
      base::BindRepeating(
          &PerformanceSidePanelCoordinator::CreatePerformanceWebUIView,
          base::Unretained(this))));
}

void PerformanceSidePanelCoordinator::Show(
    std::vector<side_panel::mojom::PerformanceSidePanelNotification>
        notifications,
    SidePanelOpenTrigger open_trigger) {
  side_panel_notifications_ = notifications;
  auto* side_panel_ui = SidePanelUI::GetSidePanelUIForBrowser(&GetBrowser());
  side_panel_ui->Show(SidePanelEntry::Id::kPerformance, open_trigger);
}

std::unique_ptr<views::View>
PerformanceSidePanelCoordinator::CreatePerformanceWebUIView() {
  // TODO(pbos): Remove the duplicate paths when/if the SidePanelModel path is
  // ready for production, or abandoned.
  static constexpr bool use_side_panel_model = false;
  if (use_side_panel_model) {
    // TODO(pbos): Move entry/registration/creation point outside views/ code.
    return std::make_unique<SidePanelModelHost>(GetPerformanceSidePanelModel());
  }
  std::vector<std::string> notifications(side_panel_notifications_.size());
  for (size_t i = 0; i < side_panel_notifications_.size(); i++) {
    notifications[i] =
        base::NumberToString(static_cast<int>(side_panel_notifications_[i]));
  }
  GURL side_panel_url = GURL(chrome::kChromeUIPerformanceSidePanelURL);
  side_panel_url = net::AppendQueryParameter(
      side_panel_url, "notifications", base::JoinString(notifications, ","));
  auto wrapper =
      std::make_unique<WebUIContentsWrapperT<PerformanceSidePanelUI>>(
          side_panel_url, GetBrowser().profile(), IDS_SHOW_PERFORMANCE,
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false);
  auto view = std::make_unique<SidePanelWebUIViewT<PerformanceSidePanelUI>>(
      base::RepeatingClosure(), base::RepeatingClosure(), std::move(wrapper));
  view->SetProperty(views::kElementIdentifierKey,
                    kPerformanceSidePanelWebViewElementId);
  return view;
}

BROWSER_USER_DATA_KEY_IMPL(PerformanceSidePanelCoordinator);
