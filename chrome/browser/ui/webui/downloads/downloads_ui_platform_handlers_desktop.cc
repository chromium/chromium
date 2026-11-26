// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/task/current_thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui_platform_handlers.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace downloads {

void NotifyDownloadOpened(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  if (auto* tab_interface =
          tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(
            tab_interface->GetBrowserWindowInterface())) {
      views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
          kDownloadedFileOpenedCustomEventId, browser_view);
    }
  }
}

void DragDownload(const download::DownloadItem* item,
                  content::WebContents* web_contents) {
  CHECK(item);
  CHECK(web_contents);
  const display::Screen* const screen = display::Screen::Get();
  gfx::NativeView view = web_contents->GetNativeView();
  gfx::Image* icon = g_browser_process->icon_manager()->LookupIconFromFilepath(
      item->GetTargetFilePath(), IconLoader::NORMAL,
      screen->GetPreferredScaleFactorForView(view).value_or(1.0f));
  {
    // Enable nested tasks during DnD, while `DragDownloadItem()` blocks.
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
    DragDownloadItem(item, icon, view);
  }
}

bool ShowEsbSettings(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents);
  if (!browser) {
    return false;
  }
  chrome::ShowSafeBrowsingEnhancedProtectionWithIph(
      browser,
      safe_browsing::SafeBrowsingSettingReferralMethod::kDownloadPageRowPromo);
  return true;
}

}  // namespace downloads
