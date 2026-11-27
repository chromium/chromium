// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_PLATFORM_HANDLERS_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_PLATFORM_HANDLERS_H_

#include "chrome/browser/download/download_item_warning_data.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

// Platform-specific hooks for the Downloads WebUI, letting the shared handler
// build on platforms without the desktop browser UI. Desktop implementations
// live in `downloads_ui_platform_handlers_desktop.cc`.
namespace downloads {

// Outcome of a download warning interaction on the downloads page, used to
// trigger platform-specific HaTS surveys. Platform-neutral subset of the
// desktop-only `DownloadWarningHatsType` enum.
enum class WarningSurveyType {
  kBypass,
  kHeed,
  kIgnore,
};

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

// Triggers a download warning HaTS survey for `item` if eligible. `item` and
// `profile` must be non-null. Does nothing if `item` is not eligible for a
// warning survey. Expected to do nothing on platforms without desktop download
// warning HaTS surveys.
void MaybeTriggerDownloadWarningHatsSurvey(download::DownloadItem* item,
                                           Profile* profile,
                                           WarningSurveyType survey_type,
                                           int num_page_warnings);

// Triggers a Trust and Safety sentiment survey for `item` if enabled. `item`
// must be non-null; `surface` must be `DOWNLOADS_PAGE` or `DOWNLOAD_PROMPT`;
// `action` must be `PROCEED` or `DISCARD`. Expected to do nothing on platforms
// where `TrustSafetySentimentService` is not used.
void MaybeTriggerTrustSafetySurvey(
    download::DownloadItem* item,
    DownloadItemWarningData::WarningSurface surface,
    DownloadItemWarningData::WarningAction action);

}  // namespace downloads

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_PLATFORM_HANDLERS_H_
