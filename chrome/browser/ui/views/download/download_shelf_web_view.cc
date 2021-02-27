// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_web_view.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "ui/views/border.h"
#include "url/url_constants.h"

DownloadShelfWebView::DownloadShelfWebView(Browser* browser)
    : DownloadShelf(browser, browser->profile()), WebView(browser->profile()) {
  // TODO: Replace with a new chrome::kChromeUIDownloadsBarURL.
  LoadInitialURL(GURL(url::kAboutBlankURL));
  SetBorder(views::CreateSolidSidedBorder(
      1, 0, 0, 0, ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR));

  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents());
  task_manager::WebContentsTags::CreateForTabContents(web_contents());
}

DownloadShelfWebView::~DownloadShelfWebView() = default;

gfx::Size DownloadShelfWebView::CalculatePreferredSize() const {
  return gfx::Size(0, 50);
}

void DownloadShelfWebView::DoShowDownload(
    DownloadUIModel::DownloadUIModelPtr download) {}

void DownloadShelfWebView::DoOpen() {}

void DownloadShelfWebView::DoClose() {}

void DownloadShelfWebView::DoHide() {}

void DownloadShelfWebView::DoUnhide() {}

bool DownloadShelfWebView::IsShowing() const {
  return GetVisible();
}

bool DownloadShelfWebView::IsClosing() const {
  return false;
}
