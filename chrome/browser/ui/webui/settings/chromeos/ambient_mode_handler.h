// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_

#include <vector>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "net/base/backoff_entry.h"

namespace ash {
struct AmbientSettings;
}  // namespace ash

namespace base {
class ListValue;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace chromeos {
namespace settings {

// Chrome OS ambient mode settings page UI handler, to allow users to customize
// photo frame and other related functionalities.
class AmbientModeHandler : public ::settings::SettingsPageUIHandler {
 public:
  AmbientModeHandler();
  AmbientModeHandler(const AmbientModeHandler&) = delete;
  AmbientModeHandler& operator=(const AmbientModeHandler&) = delete;
  ~AmbientModeHandler() override;

  // settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override;

 private:
  friend class AmbientModeHandlerTest;

  // WebUI call to request topic source and temperature unit related data.
  void HandleRequestSettings(const base::ListValue* args);

  // WebUI call to request albums related data.
  void HandleRequestAlbums(const base::ListValue* args);

  // WebUI call to sync temperature unit with server.
  void HandleSetSelectedTemperatureUnit(const base::ListValue* args);

  // WebUI call to sync albums with server.
  void HandleSetSelectedAlbums(const base::ListValue* args);

  // Send the "temperature-unit-changed" WebUIListener event to update the
  // WebUI.
  void SendTemperatureUnit();

  // Send the "topic-source-changed" WebUIListener event to update the WebUI.
  void SendTopicSource();

  // Send the "albums-changed" WebUIListener event with albums info
  // in the |topic_source|.
  void SendAlbums(ash::AmbientModeTopicSource topic_source);

  // Send the "album-preview-changed" WebUIListener event with album preview
  // in the |topic_source|.
  void SendAlbumPreview(ash::AmbientModeTopicSource topic_source,
                        const std::string& album_id,
                        std::string&& png_data_url);

  // Update the local |settings_| to server.
  void UpdateSettings();

  // Called when the settings is updated.
  void OnUpdateSettings(bool success);

  // Will be called from ambientMode/photos subpage and ambientMode subpage.
  // |topic_source| is used to request the albums in that source and identify
  // the callers:
  //   1. |kGooglePhotos|: ambientMode/photos?topicSource=0
  //   2. |kArtGallery|:   ambientMode/photos?topicSource=1
  //   3. base::nullopt:   ambientMode/
  void RequestSettingsAndAlbums(
      base::Optional<ash::AmbientModeTopicSource> topic_source);
  void OnSettingsAndAlbumsFetched(
      base::Optional<ash::AmbientModeTopicSource> topic_source,
      const base::Optional<ash::AmbientSettings>& settings,
      ash::PersonalAlbums personal_albums);

  // The |settings_| could be stale when the albums in Google Photos changes.
  // Prune the |selected_album_id| which does not exist any more.
  // Populate albums with selected info which will be shown on Settings UI.
  void SyncSettingsAndAlbums();

  // Update topic source if needed.
  void UpdateTopicSource(ash::AmbientModeTopicSource topic_source);
  void MaybeUpdateTopicSource(ash::AmbientModeTopicSource topic_source);

  void DownloadAlbumPreviewImage(ash::AmbientModeTopicSource topic_source);

  void OnAlbumPreviewImageDownloaded(ash::AmbientModeTopicSource topic_source,
                                     const std::string& album_id,
                                     const gfx::ImageSkia& image);

  ash::PersonalAlbum* FindPersonalAlbumById(const std::string& album_id);

  ash::ArtSetting* FindArtAlbumById(const std::string& album_id);

  base::Optional<ash::AmbientSettings> settings_;

  ash::PersonalAlbums personal_albums_;

  // Backoff retries for RequestSettingsAndAlbums().
  net::BackoffEntry fetch_settings_retry_backoff_;

  // Whether the Settings updating is ongoing.
  bool is_updating_backend_ = false;

  // Whether there are pending updates.
  bool has_pending_updates_for_backend_ = false;

  // Backoff retries for UpdateSettings().
  net::BackoffEntry update_settings_retry_backoff_;

  base::WeakPtrFactory<AmbientModeHandler> backend_weak_factory_{this};
  base::WeakPtrFactory<AmbientModeHandler> ui_update_weak_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_
