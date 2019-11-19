// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/arc_app_info_links_panel.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/mojom/app.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

ArcAppInfoLinksPanel::ArcAppInfoLinksPanel(Profile* profile,
                                           const extensions::Extension* app)
    : AppInfoPanel(profile, app),
      app_list_observer_(this),
      manage_link_(nullptr) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  auto manage_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_ARC_APPLICATION_INFO_MANAGE_LINK));
  manage_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  manage_link->set_listener(this);
  manage_link->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  manage_link_ = AddChildView(std::move(manage_link));

  ArcAppListPrefs* const arc_prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_prefs);
  app_list_observer_.Add(arc_prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      ArcAppListPrefs::Get(profile)->GetApp(arc::kSettingsAppId);
  if (app_info)
    UpdateLink(app_info->ready);
}

ArcAppInfoLinksPanel::~ArcAppInfoLinksPanel() {}

void ArcAppInfoLinksPanel::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(manage_link_, source);
  const int64_t display_id =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(source->GetWidget()->GetNativeView())
          .id();
  if (arc::ShowPackageInfo(
          arc::ArcIntentHelperBridge::kArcIntentHelperPackageName,
          arc::mojom::ShowPackageInfoPage::MANAGE_LINKS, display_id)) {
    Close();
  }
}

void ArcAppInfoLinksPanel::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_id == arc::kSettingsAppId)
    UpdateLink(app_info.ready);
}

void ArcAppInfoLinksPanel::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_id == arc::kSettingsAppId)
    UpdateLink(app_info.ready);
}

void ArcAppInfoLinksPanel::OnAppRemoved(const std::string& app_id) {
  if (app_id == arc::kSettingsAppId)
    UpdateLink(false);
}

void ArcAppInfoLinksPanel::UpdateLink(bool enabled) {
  manage_link_->SetEnabled(enabled);
}
