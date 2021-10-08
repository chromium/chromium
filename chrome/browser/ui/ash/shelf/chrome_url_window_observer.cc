// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_url_window_observer.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ui/ash/chrome_url_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace {

const std::u16string GetTitleForWindow(const GURL& gurl) {
  // TODO(crbug/1256494): Get GURL specific title string.
  return l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE);
}

const ash::ShelfID GetShelfIDForWindow(const GURL& gurl) {
  // TODO(crbug/1256494): Get URL specific app ID which describes e.g. the used
  // icon.
  return ash::ShelfID(ash::kInternalAppIdSettings);
}

// A helper class that updates the title of Chrome OS Chrome:// browser windows.
class AuraWindowChromeUrlTitleTracker : public aura::WindowTracker {
 public:
  AuraWindowChromeUrlTitleTracker() = default;
  AuraWindowChromeUrlTitleTracker(const AuraWindowChromeUrlTitleTracker&) =
      delete;
  AuraWindowChromeUrlTitleTracker& operator=(
      const AuraWindowChromeUrlTitleTracker&) = delete;

  ~AuraWindowChromeUrlTitleTracker() override;

  // aura::WindowTracker:
  void OnWindowTitleChanged(aura::Window* window) override {
    // Name the window according to GURL instead of "Google Chrome - <...>".
    Browser* browser = chrome::FindBrowserWithWindow(window);
    DCHECK(browser);
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetWebContentsAt(0);
    DCHECK(web_contents);
    window->SetTitle(GetTitleForWindow(web_contents->GetURL()));
  }
};

AuraWindowChromeUrlTitleTracker::~AuraWindowChromeUrlTitleTracker() = default;

}  // namespace

ChromeUrlWindowObserver::ChromeUrlWindowObserver(
    ChromeUrlWindowManager* window_manager) {
  aura_window_tracker_ = std::make_unique<AuraWindowChromeUrlTitleTracker>();
  observation_.Observe(window_manager);
}

ChromeUrlWindowObserver::~ChromeUrlWindowObserver() = default;

void ChromeUrlWindowObserver::OnNewChromeUrlWindow(
    Browser* chrome_url_browser) {
  aura::Window* window = chrome_url_browser->window()->GetNativeWindow();
  content::WebContents* web_contents =
      chrome_url_browser->tab_strip_model()->GetWebContentsAt(0);
  DCHECK(web_contents);
  const GURL& gurl = web_contents->GetURL();

  window->SetTitle(GetTitleForWindow(gurl));
  const ash::ShelfID shelf_id = GetShelfIDForWindow(gurl);
  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(ash::kAppIDKey, shelf_id.app_id);
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
  aura_window_tracker_->Add(window);
}
