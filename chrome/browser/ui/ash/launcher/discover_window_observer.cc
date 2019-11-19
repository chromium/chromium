// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/discover_window_observer.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// A helper class that updates the title of Chrome OS Discover browser windows.
class AuraWindowDiscoverTitleTracker : public aura::WindowTracker {
 public:
  AuraWindowDiscoverTitleTracker() = default;
  ~AuraWindowDiscoverTitleTracker() override = default;

  // aura::WindowTracker:
  void OnWindowTitleChanged(aura::Window* window) override {
    // Name the window "Discover" instead of "Google Chrome - Discover".
    window->SetTitle(l10n_util::GetStringUTF16(IDS_INTERNAL_APP_DISCOVER));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AuraWindowDiscoverTitleTracker);
};

}  // namespace

DiscoverWindowObserver::DiscoverWindowObserver() {
  aura_window_tracker_ = std::make_unique<AuraWindowDiscoverTitleTracker>();
  chromeos::DiscoverWindowManager::GetInstance()->AddObserver(this);
}

DiscoverWindowObserver::~DiscoverWindowObserver() {
  chromeos::DiscoverWindowManager::GetInstance()->RemoveObserver(this);
}

void DiscoverWindowObserver::OnNewDiscoverWindow(Browser* discover_browser) {
  aura::Window* window = discover_browser->window()->GetNativeWindow();
  window->SetTitle(l10n_util::GetStringUTF16(IDS_INTERNAL_APP_DISCOVER));
  const ash::ShelfID shelf_id(ash::kInternalAppIdDiscover);
  window->SetProperty(ash::kAppIDKey, shelf_id.app_id);
  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
  aura_window_tracker_->Add(window);
}
