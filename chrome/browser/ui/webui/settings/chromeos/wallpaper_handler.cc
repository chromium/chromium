// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/wallpaper_handler.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/backdrop_wallpaper_handlers/backdrop_wallpaper.pb.h"
#include "chrome/browser/chromeos/backdrop_wallpaper_handlers/backdrop_wallpaper_handlers.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {
namespace settings {

WallpaperHandler::WallpaperHandler() = default;

WallpaperHandler::~WallpaperHandler() = default;

void WallpaperHandler::OnJavascriptAllowed() {}
void WallpaperHandler::OnJavascriptDisallowed() {}

void WallpaperHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openWallpaperManager",
      base::BindRepeating(&WallpaperHandler::HandleOpenWallpaperManager,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "isWallpaperSettingVisible",
      base::BindRepeating(&WallpaperHandler::HandleIsWallpaperSettingVisible,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "isWallpaperPolicyControlled",
      base::BindRepeating(&WallpaperHandler::HandleIsWallpaperPolicyControlled,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "fetchWallpaperCollections",
      base::BindRepeating(&WallpaperHandler::HandleFetchWallpaperCollections,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "fetchImagesForCollection",
      base::BindRepeating(&WallpaperHandler::HandleFetchCollectionImages,
                          base::Unretained(this)));
}

void WallpaperHandler::HandleIsWallpaperSettingVisible(
    const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  ResolveCallback(
      args->GetList()[0],
      WallpaperControllerClient::Get()->ShouldShowWallpaperSetting());
}

void WallpaperHandler::HandleIsWallpaperPolicyControlled(
    const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  bool result = WallpaperControllerClient::Get()
                    ->IsActiveUserWallpaperControlledByPolicy();
  ResolveCallback(args->GetList()[0], result);
}

void WallpaperHandler::HandleOpenWallpaperManager(const base::ListValue* args) {
  WallpaperControllerClient::Get()->OpenWallpaperPickerIfAllowed();
}

void WallpaperHandler::HandleFetchWallpaperCollections(
    const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  DCHECK(IsJavascriptAllowed()) << "Page should already be initialized";
  backdrop_api_weak_factory_.InvalidateWeakPtrs();
  collection_info_fetcher_.Start(
      base::BindOnce(&WallpaperHandler::OnFetchWallpaperCollections,
                     backdrop_api_weak_factory_.GetWeakPtr(),
                     args->GetList().front().Clone()));
}

void WallpaperHandler::OnFetchWallpaperCollections(
    const base::Value& callback_id,
    bool success,
    const std::vector<backdrop::Collection>& collections) {
  if (!success || collections.empty()) {
    RejectJavascriptCallback(callback_id, base::Value(base::Value::Type::NONE));
    return;
  }

  base::Value result(base::Value::Type::LIST);
  for (const auto& collection : collections) {
    base::Value item(base::Value::Type::DICTIONARY);
    item.SetKey("id", base::Value(collection.collection_id()));
    item.SetKey("name", base::Value(collection.collection_name()));
    result.Append(std::move(item));
  }
  ResolveJavascriptCallback(callback_id, result);
}

void WallpaperHandler::HandleFetchCollectionImages(
    const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 2U);
  DCHECK(IsJavascriptAllowed()) << "Page should already be initialized";

  const base::Value& last_arg = args->GetList().back();
  DCHECK(last_arg.is_string()) << "Last argument must be the collection id";
  const std::string& collection_id = last_arg.GetString();

  backdrop_api_weak_factory_.InvalidateWeakPtrs();
  collection_images_fetcher_ =
      std::make_unique<backdrop_wallpaper_handlers::ImageInfoFetcher>(
          collection_id);

  collection_images_fetcher_->Start(
      base::BindOnce(&WallpaperHandler::OnFetchCollectionImages,
                     backdrop_api_weak_factory_.GetWeakPtr(),
                     args->GetList().front().Clone()));
}

void WallpaperHandler::OnFetchCollectionImages(
    const base::Value& callback_id,
    bool success,
    const std::vector<backdrop::Image>& images) {
  if (!success || images.empty()) {
    RejectJavascriptCallback(callback_id, base::Value(base::Value::Type::NONE));
    return;
  }

  base::Value result(base::Value::Type::LIST);
  for (const auto& image : images) {
    base::Value item(base::Value::Type::DICTIONARY);
    item.SetKey("url", base::Value(image.image_url()));
    result.Append(std::move(item));
  }
  ResolveJavascriptCallback(callback_id, result);
}

void WallpaperHandler::ResolveCallback(const base::Value& callback_id,
                                       bool result) {
  AllowJavascript();
  ResolveJavascriptCallback(callback_id, base::Value(result));
}

}  // namespace settings
}  // namespace chromeos
