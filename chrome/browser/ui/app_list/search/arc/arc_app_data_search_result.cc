// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_data_search_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace app_list {

namespace {

constexpr char kAppDataSearchPrefix[] = "appdatasearch://";

constexpr int kAvatarSize = 40;

bool LaunchIntent(const std::string& intent_uri, int64_t display_id) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;

  auto* app = arc_service_manager->arc_bridge_service()->app();

  if (auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(app, LaunchIntent)) {
    app_instance->LaunchIntent(intent_uri, display_id);
    return true;
  }

  if (auto* app_instance =
          ARC_GET_INSTANCE_FOR_METHOD(app, LaunchIntentDeprecated)) {
    app_instance->LaunchIntentDeprecated(intent_uri, base::nullopt);
    return true;
  }

  return false;
}

// Provide circular avatar image source.
class AvatarImageSource : public gfx::CanvasImageSource {
 public:
  AvatarImageSource(gfx::ImageSkia avatar, int size)
      : CanvasImageSource(gfx::Size(size, size)), radius_(size / 2) {
    avatar_ = gfx::ImageSkiaOperations::CreateResizedImage(
        avatar, skia::ImageOperations::RESIZE_BEST, gfx::Size(size, size));
  }
  ~AvatarImageSource() override = default;

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    SkPath circular_mask;
    circular_mask.addCircle(SkIntToScalar(radius_), SkIntToScalar(radius_),
                            SkIntToScalar(radius_));
    canvas->ClipPath(circular_mask, true);
    canvas->DrawImageInt(avatar_, 0, 0);
  }

  gfx::ImageSkia avatar_;
  const int radius_;

  DISALLOW_COPY_AND_ASSIGN(AvatarImageSource);
};

}  // namespace

ArcAppDataSearchResult::ArcAppDataSearchResult(
    arc::mojom::AppDataResultPtr data,
    AppListControllerDelegate* list_controller)
    : data_(std::move(data)), list_controller_(list_controller) {
  SetTitle(base::UTF8ToUTF16(data_->label));
  set_id(kAppDataSearchPrefix + launch_intent_uri());
  if (data_->type == arc::mojom::AppDataResultType::PERSON) {
    SetDisplayType(ash::SearchResultDisplayType::kTile);
  } else if (data_->type == arc::mojom::AppDataResultType::NOTE_DOCUMENT) {
    SetDetails(base::UTF8ToUTF16(data_->text));
    SetDisplayType(ash::SearchResultDisplayType::kList);
  }

  // TODO(warx): set default images when icon_png_data() is not available.
  if (!icon_png_data()) {
    SetIcon(gfx::ImageSkia());
    return;
  }

  icon_decode_request_ = std::make_unique<arc::IconDecodeRequest>(
      base::BindOnce(&ArcAppDataSearchResult::ApplyIcon,
                     weak_ptr_factory_.GetWeakPtr()),
      ash::AppListConfig::instance().search_tile_icon_dimension());
  icon_decode_request_->StartWithOptions(icon_png_data().value());
}

ArcAppDataSearchResult::~ArcAppDataSearchResult() = default;

void ArcAppDataSearchResult::GetContextMenuModel(
    GetMenuModelCallback callback) {
  // TODO(warx): Enable Context Menu.
  std::move(callback).Run(nullptr);
}

void ArcAppDataSearchResult::Open(int event_flags) {
  LaunchIntent(launch_intent_uri(), list_controller_->GetAppListDisplayId());
}

void ArcAppDataSearchResult::ApplyIcon(const gfx::ImageSkia& icon) {
  if (data_->type == arc::mojom::AppDataResultType::PERSON) {
    SetIcon(
        gfx::ImageSkia(std::make_unique<AvatarImageSource>(icon, kAvatarSize),
                       gfx::Size(kAvatarSize, kAvatarSize)));
    return;
  }
  SetIcon(icon);
}

ash::SearchResultType ArcAppDataSearchResult::GetSearchResultType() const {
  switch (data_->type) {
    case arc::mojom::AppDataResultType::PERSON:
      return ash::APP_DATA_RESULT_PERSON;
    case arc::mojom::AppDataResultType::NOTE_DOCUMENT:
      return ash::APP_DATA_RESULT_NOTE_DOCUMENT;
    default:
      NOTREACHED();
      return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }
}

}  // namespace app_list
