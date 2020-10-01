// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace chromeos {
namespace settings {

namespace {

// Width and height of the preview image for personal album.
constexpr int kBannerWidthPx = 160;
constexpr int kBannerHeightPx = 160;

// Strings for converting to and from AmbientModeTemperatureUnit enum.
constexpr char kCelsius[] = "celsius";
constexpr char kFahrenheit[] = "fahrenheit";

constexpr int kMaxRetries = 3;

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,          // Number of initial errors to ignore.
    500,        // Initial delay in ms.
    2.0,        // Factor by which the waiting time will be multiplied.
    0.2,        // Fuzzing percentage.
    60 * 1000,  // Maximum delay in ms.
    -1,         // Never discard the entry.
    true,       // Use initial delay.
};

ash::AmbientModeTemperatureUnit ExtractTemperatureUnit(
    const base::ListValue* args) {
  auto temperature_unit = args->GetList()[0].GetString();
  if (temperature_unit == kCelsius) {
    return ash::AmbientModeTemperatureUnit::kCelsius;
  } else if (temperature_unit == kFahrenheit) {
    return ash::AmbientModeTemperatureUnit::kFahrenheit;
  }
  NOTREACHED() << "Unknown temperature unit";
  return ash::AmbientModeTemperatureUnit::kFahrenheit;
}

std::string TemperatureUnitToString(
    ash::AmbientModeTemperatureUnit temperature_unit) {
  switch (temperature_unit) {
    case ash::AmbientModeTemperatureUnit::kFahrenheit:
      return kFahrenheit;
    case ash::AmbientModeTemperatureUnit::kCelsius:
      return kCelsius;
  }
}

ash::AmbientModeTopicSource ExtractTopicSource(const base::Value& value) {
  ash::AmbientModeTopicSource topic_source =
      static_cast<ash::AmbientModeTopicSource>(value.GetInt());
  // Check the |topic_source| has valid value.
  CHECK_GE(topic_source, ash::AmbientModeTopicSource::kMinValue);
  CHECK_LE(topic_source, ash::AmbientModeTopicSource::kMaxValue);
  return topic_source;
}

ash::AmbientModeTopicSource ExtractTopicSource(const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  return ExtractTopicSource(args->GetList()[0]);
}

void EncodeImage(const gfx::ImageSkia& image,
                 std::vector<unsigned char>* output) {
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(*image.bitmap(),
                                         /*discard_transparency=*/false,
                                         output)) {
    VLOG(1) << "Failed to encode image to png";
    output->clear();
  }
}

base::string16 GetAlbumDescription(const ash::PersonalAlbum& album) {
  if (album.album_id == ash::kAmbientModeRecentHighlightsAlbumId) {
    return l10n_util::GetStringUTF16(
        IDS_OS_SETTINGS_AMBIENT_MODE_ALBUMS_SUBPAGE_RECENT_DESC);
  }

  if (album.number_of_photos <= 1) {
    return l10n_util::GetStringFUTF16Int(
        IDS_OS_SETTINGS_AMBIENT_MODE_ALBUMS_SUBPAGE_PHOTOS_NUM_SINGULAR,
        album.number_of_photos);
  }

  return l10n_util::GetStringFUTF16Int(
      IDS_OS_SETTINGS_AMBIENT_MODE_ALBUMS_SUBPAGE_PHOTOS_NUM_PLURAL,
      album.number_of_photos);
}

}  // namespace

AmbientModeHandler::AmbientModeHandler()
    : fetch_settings_retry_backoff_(&kRetryBackoffPolicy),
      update_settings_retry_backoff_(&kRetryBackoffPolicy) {}

AmbientModeHandler::~AmbientModeHandler() = default;

void AmbientModeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestSettings",
      base::BindRepeating(&AmbientModeHandler::HandleRequestSettings,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestAlbums",
      base::BindRepeating(&AmbientModeHandler::HandleRequestAlbums,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setSelectedTemperatureUnit",
      base::BindRepeating(&AmbientModeHandler::HandleSetSelectedTemperatureUnit,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setSelectedAlbums",
      base::BindRepeating(&AmbientModeHandler::HandleSetSelectedAlbums,
                          base::Unretained(this)));
}

void AmbientModeHandler::OnJavascriptDisallowed() {
  backend_weak_factory_.InvalidateWeakPtrs();
  ui_update_weak_factory_.InvalidateWeakPtrs();
}

void AmbientModeHandler::HandleRequestSettings(const base::ListValue* args) {
  CHECK(args);
  CHECK(args->empty());

  AllowJavascript();

  // Settings subpages may have changed from ambientMode/photos to ambientMode
  // since the last time requesting the data. Abort any request in progress to
  // avoid unnecessary updating invisible subpage.
  ui_update_weak_factory_.InvalidateWeakPtrs();
  RequestSettingsAndAlbums(/*topic_source=*/base::nullopt);
}

void AmbientModeHandler::HandleRequestAlbums(const base::ListValue* args) {
  CHECK(args);
  CHECK_EQ(args->GetSize(), 1U);

  AllowJavascript();

  // ambientMode/photos subpages may have changed, e.g. from displaying Google
  // Photos to Art gallery, since the last time requesting the data.
  // Abort any request in progress to avoid updating incorrect contents.
  ui_update_weak_factory_.InvalidateWeakPtrs();
  RequestSettingsAndAlbums(ExtractTopicSource(args));
}

void AmbientModeHandler::HandleSetSelectedTemperatureUnit(
    const base::ListValue* args) {
  DCHECK(settings_);
  CHECK_EQ(1U, args->GetSize());

  settings_->temperature_unit = ExtractTemperatureUnit(args);
  UpdateSettings();
}

void AmbientModeHandler::HandleSetSelectedAlbums(const base::ListValue* args) {
  const base::DictionaryValue* dictionary = nullptr;
  CHECK(!args->GetList().empty());
  args->GetList()[0].GetAsDictionary(&dictionary);
  CHECK(dictionary);

  const base::Value* topic_source_value = dictionary->FindKey("topicSource");
  CHECK(topic_source_value);
  ash::AmbientModeTopicSource topic_source =
      ExtractTopicSource(*topic_source_value);
  const base::Value* albums = dictionary->FindKey("albums");
  CHECK(albums);
  switch (topic_source) {
    case ash::AmbientModeTopicSource::kGooglePhotos:
      // For Google Photos, we will populate the |selected_album_ids| with IDs
      // of selected albums.
      settings_->selected_album_ids.clear();
      for (const auto& album : albums->GetList()) {
        const base::Value* album_id = album.FindKey("albumId");
        const std::string& id = album_id->GetString();
        ash::PersonalAlbum* personal_album = FindPersonalAlbumById(id);
        DCHECK(personal_album);
        settings_->selected_album_ids.emplace_back(personal_album->album_id);
      }

      // Update topic source based on selections.
      if (settings_->selected_album_ids.empty())
        settings_->topic_source = ash::AmbientModeTopicSource::kArtGallery;
      else
        settings_->topic_source = ash::AmbientModeTopicSource::kGooglePhotos;
      break;
    case ash::AmbientModeTopicSource::kArtGallery:
      // For Art gallery, we set the corresponding setting to be enabled or not
      // based on the selections.
      for (auto& art_setting : settings_->art_settings) {
        const std::string& album_id = art_setting.album_id;
        auto it = std::find_if(
            albums->GetList().begin(), albums->GetList().end(),
            [&album_id](const auto& album) {
              return album.FindKey("albumId")->GetString() == album_id;
            });
        const bool checked = it != albums->GetList().end();
        art_setting.enabled = checked;
        // A setting must be visible to be enabled.
        if (art_setting.enabled)
          CHECK(art_setting.visible);
      }
      break;
  }

  UpdateSettings();
  // TODO(wutao): Undate the UI when success in OnUpdateSettings.
  SendTopicSource();
}

void AmbientModeHandler::SendTemperatureUnit() {
  DCHECK(settings_);
  FireWebUIListener(
      "temperature-unit-changed",
      base::Value(TemperatureUnitToString(settings_->temperature_unit)));
}

void AmbientModeHandler::SendTopicSource() {
  DCHECK(settings_);
  base::Value topic_source(base::Value::Type::DICTIONARY);
  topic_source.SetKey("hasGooglePhotosAlbums",
                      base::Value(!personal_albums_.albums.empty()));
  topic_source.SetKey("topicSource",
                      base::Value(static_cast<int>(settings_->topic_source)));
  FireWebUIListener("topic-source-changed",
                    base::Value(std::move(topic_source)));
}

void AmbientModeHandler::SendAlbums(ash::AmbientModeTopicSource topic_source) {
  DCHECK(settings_);

  base::Value dictionary(base::Value::Type::DICTIONARY);
  base::Value albums(base::Value::Type::LIST);
  switch (topic_source) {
    case ash::AmbientModeTopicSource::kGooglePhotos:
      for (const auto& album : personal_albums_.albums) {
        base::Value value(base::Value::Type::DICTIONARY);
        value.SetKey("albumId", base::Value(album.album_id));
        value.SetKey("checked", base::Value(album.selected));
        value.SetKey("description", base::Value(GetAlbumDescription(album)));
        value.SetKey("title", base::Value(album.album_name));
        value.SetKey("url", base::Value(album.png_data_url));
        albums.Append(std::move(value));
      }
      break;
    case ash::AmbientModeTopicSource::kArtGallery:
      for (const auto& setting : settings_->art_settings) {
        if (!setting.visible)
          continue;
        base::Value value(base::Value::Type::DICTIONARY);
        value.SetKey("albumId", base::Value(setting.album_id));
        value.SetKey("checked", base::Value(setting.enabled));
        value.SetKey("description", base::Value(setting.description));
        value.SetKey("title", base::Value(setting.title));
        value.SetKey("url", base::Value(setting.png_data_url));
        albums.Append(std::move(value));
      }
      break;
  }

  dictionary.SetKey("topicSource", base::Value(static_cast<int>(topic_source)));
  dictionary.SetKey("selectedTopicSource",
                    base::Value(static_cast<int>(settings_->topic_source)));
  dictionary.SetKey("albums", std::move(albums));
  FireWebUIListener("albums-changed", std::move(dictionary));
}

void AmbientModeHandler::SendAlbumPreview(
    ash::AmbientModeTopicSource topic_source,
    const std::string& album_id,
    std::string&& png_data_url) {
  base::Value album(base::Value::Type::DICTIONARY);
  album.SetKey("albumId", base::Value(album_id));
  album.SetKey("topicSource", base::Value(static_cast<int>(topic_source)));
  album.SetKey("url", base::Value(png_data_url));
  FireWebUIListener("album-preview-changed", std::move(album));
}

void AmbientModeHandler::UpdateSettings() {
  if (is_updating_backend_) {
    has_pending_updates_for_backend_ = true;
    return;
  }

  has_pending_updates_for_backend_ = false;
  is_updating_backend_ = true;

  DCHECK(settings_);
  ash::AmbientBackendController::Get()->UpdateSettings(
      *settings_, base::BindOnce(&AmbientModeHandler::OnUpdateSettings,
                                 backend_weak_factory_.GetWeakPtr()));
}

void AmbientModeHandler::OnUpdateSettings(bool success) {
  is_updating_backend_ = false;

  if (success) {
    update_settings_retry_backoff_.Reset();
  } else {
    update_settings_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    if (update_settings_retry_backoff_.failure_count() > kMaxRetries)
      return;
  }

  if (has_pending_updates_for_backend_ || !success) {
    const base::TimeDelta kDelay =
        update_settings_retry_backoff_.GetTimeUntilRelease();
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AmbientModeHandler::UpdateSettings,
                       backend_weak_factory_.GetWeakPtr()),
        kDelay);
  }
}

void AmbientModeHandler::RequestSettingsAndAlbums(
    base::Optional<ash::AmbientModeTopicSource> topic_source) {
  // TODO(b/161044021): Add a helper function to get all the albums. Currently
  // only load 100 latest modified albums.
  ash::AmbientBackendController::Get()->FetchSettingsAndAlbums(
      kBannerWidthPx, kBannerHeightPx, /*num_albums=*/100,
      base::BindOnce(&AmbientModeHandler::OnSettingsAndAlbumsFetched,
                     ui_update_weak_factory_.GetWeakPtr(), topic_source));
}

void AmbientModeHandler::OnSettingsAndAlbumsFetched(
    base::Optional<ash::AmbientModeTopicSource> topic_source,
    const base::Optional<ash::AmbientSettings>& settings,
    ash::PersonalAlbums personal_albums) {
  // |settings| value implies success.
  if (!settings) {
    fetch_settings_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    if (fetch_settings_retry_backoff_.failure_count() > kMaxRetries)
      return;

    const base::TimeDelta kDelay =
        fetch_settings_retry_backoff_.GetTimeUntilRelease();
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AmbientModeHandler::RequestSettingsAndAlbums,
                       ui_update_weak_factory_.GetWeakPtr(), topic_source),
        kDelay);
    return;
  }

  fetch_settings_retry_backoff_.Reset();
  settings_ = settings;
  personal_albums_ = std::move(personal_albums);
  SyncSettingsAndAlbums();

  if (!topic_source) {
    SendTopicSource();
    SendTemperatureUnit();

    // Explicitly enable the weather settings if necessary to make sure we
    // can always get weather info in the response. Leaving this settings as
    // default could result in unpredictable behavior (b/158630188). Note that
    // right now the weather info is designed to be always shown on ambient
    // screen, so we don't expose an option in ambient Settings for users to
    // switch it off.
    if (!settings_->show_weather) {
      settings_->show_weather = true;
      UpdateSettings();
    }
    return;
  }

  if (chromeos::features::IsAmbientModePhotoPreviewEnabled())
    DownloadAlbumPreviewImage(*topic_source);

  UpdateTopicSource(*topic_source);
  SendAlbums(*topic_source);
}

void AmbientModeHandler::SyncSettingsAndAlbums() {
  auto it = settings_->selected_album_ids.begin();
  while (it != settings_->selected_album_ids.end()) {
    const std::string& album_id = *it;
    ash::PersonalAlbum* album = FindPersonalAlbumById(album_id);
    if (album) {
      album->selected = true;
      ++it;
    } else {
      // The selected album does not exist any more.
      it = settings_->selected_album_ids.erase(it);
    }
  }

  if (settings_->selected_album_ids.empty())
    MaybeUpdateTopicSource(ash::AmbientModeTopicSource::kArtGallery);
}

void AmbientModeHandler::UpdateTopicSource(
    ash::AmbientModeTopicSource topic_source) {
  // If this is an Art gallery album page, will select art gallery topic source.
  if (topic_source == ash::AmbientModeTopicSource::kArtGallery) {
    MaybeUpdateTopicSource(topic_source);
    return;
  }

  // If this is a Google Photos album page, will
  // 1. Select art gallery topic source if no albums or no album is selected.
  if (settings_->selected_album_ids.empty()) {
    MaybeUpdateTopicSource(ash::AmbientModeTopicSource::kArtGallery);
    return;
  }

  // 2. Select Google Photos topic source if at least one album is selected.
  MaybeUpdateTopicSource(ash::AmbientModeTopicSource::kGooglePhotos);
}

void AmbientModeHandler::MaybeUpdateTopicSource(
    ash::AmbientModeTopicSource topic_source) {
  // If the setting is the same, no need to update.
  if (settings_->topic_source == topic_source)
    return;

  settings_->topic_source = topic_source;
  UpdateSettings();
  // TODO(wutao): Undate the UI when success in OnUpdateSettings.
  SendTopicSource();
}

void AmbientModeHandler::DownloadAlbumPreviewImage(
    ash::AmbientModeTopicSource topic_source) {
  switch (topic_source) {
    case ash::AmbientModeTopicSource::kGooglePhotos:
      // TODO(b/163413738): Slow down the downloading when there are too many
      // albums.
      for (const auto& album : personal_albums_.albums) {
        ash::ImageDownloader::Get()->Download(
            GURL(album.banner_image_url), NO_TRAFFIC_ANNOTATION_YET,
            base::BindOnce(&AmbientModeHandler::OnAlbumPreviewImageDownloaded,
                           backend_weak_factory_.GetWeakPtr(), topic_source,
                           album.album_id));
      }
      break;
    case ash::AmbientModeTopicSource::kArtGallery:
      for (const auto& album : settings_->art_settings) {
        ash::ImageDownloader::Get()->Download(
            GURL(album.preview_image_url), NO_TRAFFIC_ANNOTATION_YET,
            base::BindOnce(&AmbientModeHandler::OnAlbumPreviewImageDownloaded,
                           backend_weak_factory_.GetWeakPtr(), topic_source,
                           album.album_id));
      }
      break;
  }
}

void AmbientModeHandler::OnAlbumPreviewImageDownloaded(
    ash::AmbientModeTopicSource topic_source,
    const std::string& album_id,
    const gfx::ImageSkia& image) {
  switch (topic_source) {
    case ash::AmbientModeTopicSource::kGooglePhotos:
      // Album does not exist any more.
      if (!FindPersonalAlbumById(album_id))
        return;
      break;
    case ash::AmbientModeTopicSource::kArtGallery:
      if (!FindArtAlbumById(album_id))
        return;
      break;
  }

  std::vector<unsigned char> encoded_image_bytes;
  EncodeImage(image, &encoded_image_bytes);
  if (encoded_image_bytes.empty())
    return;

  SendAlbumPreview(topic_source, album_id,
                   webui::GetPngDataUrl(&encoded_image_bytes.front(),
                                        encoded_image_bytes.size()));
}

ash::PersonalAlbum* AmbientModeHandler::FindPersonalAlbumById(
    const std::string& album_id) {
  auto it = std::find_if(
      personal_albums_.albums.begin(), personal_albums_.albums.end(),
      [&album_id](const auto& album) { return album.album_id == album_id; });

  if (it == personal_albums_.albums.end())
    return nullptr;

  return &(*it);
}

ash::ArtSetting* AmbientModeHandler::FindArtAlbumById(
    const std::string& album_id) {
  auto it = std::find_if(
      settings_->art_settings.begin(), settings_->art_settings.end(),
      [&album_id](const auto& album) { return album.album_id == album_id; });
  // Album does not exist any more.
  if (it == settings_->art_settings.end())
    return nullptr;

  return &(*it);
}

}  // namespace settings
}  // namespace chromeos
