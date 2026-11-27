// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_PLATFORM_HANDLERS_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_PLATFORM_HANDLERS_H_

// Platform-specific hooks for the Downloads WebUI, letting the shared handler
// build on platforms without the desktop browser UI. Desktop implementations
// live in `downloads_ui_platform_handlers_desktop.cc`.

namespace content {
class WebContents;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

namespace downloads {

// Notifies platform-specific listeners that a downloaded file is being opened
// from the downloads page. `web_contents` may be null. Expected to do nothing
// on platforms without browser windows.
void NotifyDownloadOpened(content::WebContents* web_contents);

// Initiates a drag-and-drop operation for `item` from `web_contents`. `item`
// must be non-null and complete, and `web_contents` must be non-null. Expected
// to do nothing on platforms without drag and drop.
void DragDownload(const download::DownloadItem* item,
                  content::WebContents* web_contents);

// Opens the Enhanced Safe Browsing settings page with IPH for the browser
// hosting `web_contents`. `web_contents` may be null. Returns false if
// `web_contents` is null or not hosted in a browser window, or the platform has
// no ESB settings page, in which case the caller should not record the click.
bool ShowEsbSettings(content::WebContents* web_contents);

// Prompts the user for a password to deep-scan an encrypted archive `item`
// via the download bubble for the browser hosting `web_contents`. `item` must
// be non-null; `web_contents` may be null. Does nothing if `web_contents` is
// null or not hosted in a browser window. Expected to do nothing on platforms
// without the download bubble (including ChromeOS and Android).
void PromptForScanningPassword(download::DownloadItem* item,
                               content::WebContents* web_contents);

// Starts a deep scan for `item`. `item` must be non-null. Expected to do
// nothing on platforms where the downloads page never offers a deep scan
// prompt.
void StartDeepScan(download::DownloadItem* item);

// Bypasses deep scanning for `item` without opening the file. `item` must be
// non-null. Expected to do nothing on platforms where the downloads page never
// offers a deep scan prompt.
void BypassDeepScan(download::DownloadItem* item);

}  // namespace downloads

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_PLATFORM_HANDLERS_H_
