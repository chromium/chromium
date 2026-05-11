// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/tabs_from_other_devices/synced_screenshot_data_source.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

SyncedScreenshotDataSource::SyncedScreenshotDataSource() = default;

SyncedScreenshotDataSource::~SyncedScreenshotDataSource() = default;

std::string SyncedScreenshotDataSource::GetSource() {
  return "synced-screenshot";
}

void SyncedScreenshotDataSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  content::WebContents* web_contents = wc_getter.Run();
  if (!web_contents) {
    std::move(callback).Run(nullptr);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetForProfile(profile);
  if (!session_sync_service) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string_view path = url.path();

  // Path format: /<session_tag>/<tab_id>
  size_t slash_pos = path.find_last_of('/');
  if (slash_pos == std::string_view::npos || slash_pos == 0) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string_view session_tag = path.substr(1, slash_pos - 1);
  std::string_view tab_id_str = path.substr(slash_pos + 1);

  int32_t tab_id_val = 0;
  if (!base::StringToInt(tab_id_str, &tab_id_val) ||
      !SessionID::IsValidValue(tab_id_val)) {
    std::move(callback).Run(nullptr);
    return;
  }

  SessionID tab_id = SessionID::FromSerializedValue(tab_id_val);

  session_sync_service->ReadTabScreenshot(
      std::string(session_tag), tab_id,
      base::BindOnce(
          [](content::URLDataSource::GotDataCallback callback,
             std::optional<std::string> screenshot_data) {
            if (!screenshot_data) {
              std::move(callback).Run(nullptr);
              return;
            }
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::move(*screenshot_data)));
          },
          std::move(callback)));
}

std::string SyncedScreenshotDataSource::GetMimeType(const GURL& url) {
  return "image/jpg";
}

bool SyncedScreenshotDataSource::ShouldReplaceExistingSource() {
  return false;
}
