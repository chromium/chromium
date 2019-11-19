// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_playstore_app_context_menu.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/crx_file/id_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// The id prefix to identify a Play Store search result.
constexpr char kPlayAppPrefix[] = "play://";
// Badge icon color.
constexpr SkColor kBadgeColor = gfx::kGoogleGrey700;
// Size of the vector icon inside the badge.
constexpr int kBadgeIconSize = 12;
// Padding around the circular background of the badge.
constexpr int kBadgePadding = 1;

// The background image source for badge.
class BadgeBackgroundImageSource : public gfx::CanvasImageSource {
 public:
  explicit BadgeBackgroundImageSource(int size, float padding)
      : CanvasImageSource(gfx::Size(size, size)), padding_(padding) {}
  ~BadgeBackgroundImageSource() override = default;

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setColor(SK_ColorWHITE);
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    const float origin = static_cast<float>(size().width()) / 2;
    canvas->DrawCircle(gfx::PointF(origin, origin), origin - padding_, flags);
  }

  const float padding_;

  DISALLOW_COPY_AND_ASSIGN(BadgeBackgroundImageSource);
};

gfx::ImageSkia CreateBadgeIcon(const gfx::VectorIcon& vector_icon,
                               int badge_size,
                               int padding,
                               int icon_size,
                               SkColor icon_color) {
  gfx::ImageSkia background(
      std::make_unique<BadgeBackgroundImageSource>(badge_size, padding),
      gfx::Size(badge_size, badge_size));

  gfx::ImageSkia foreground(
      gfx::CreateVectorIcon(vector_icon, icon_size, icon_color));

  return gfx::ImageSkiaOperations::CreateSuperimposedImage(background,
                                                           foreground);
}

bool LaunchIntent(const std::string& intent_uri, int64_t display_id) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;

  auto* arc_bridge = arc_service_manager->arc_bridge_service();

  if (auto* app_instance =
          ARC_GET_INSTANCE_FOR_METHOD(arc_bridge->app(), LaunchIntent)) {
    app_instance->LaunchIntent(intent_uri, display_id);
    return true;
  }

  if (auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge->app(), LaunchIntentDeprecated)) {
    app_instance->LaunchIntentDeprecated(intent_uri, base::nullopt);
    return true;
  }

  return false;
}

}  // namespace

namespace app_list {

ArcPlayStoreSearchResult::ArcPlayStoreSearchResult(
    arc::mojom::AppDiscoveryResultPtr data,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : data_(std::move(data)),
      profile_(profile),
      list_controller_(list_controller) {
  SetTitle(base::UTF8ToUTF16(label().value()));
  set_id(kPlayAppPrefix +
         crx_file::id_util::GenerateId(install_intent_uri().value()));
  SetDisplayType(ash::SearchResultDisplayType::kTile);
  SetBadgeIcon(CreateBadgeIcon(
      is_instant_app() ? ash::kBadgeInstantIcon : ash::kBadgePlayIcon,
      ash::AppListConfig::instance().search_tile_badge_icon_dimension(),
      kBadgePadding, kBadgeIconSize, kBadgeColor));
  SetFormattedPrice(base::UTF8ToUTF16(formatted_price().value()));
  SetRating(review_score());
  SetResultType(is_instant_app() ? ash::AppListSearchResultType::kInstantApp
                                 : ash::AppListSearchResultType::kPlayStoreApp);

  icon_decode_request_ = std::make_unique<arc::IconDecodeRequest>(
      base::BindOnce(&ArcPlayStoreSearchResult::SetIcon,
                     weak_ptr_factory_.GetWeakPtr()),
      ash::AppListConfig::instance().search_tile_icon_dimension());
  icon_decode_request_->set_normalized(true);
  icon_decode_request_->StartWithOptions(icon_png_data());
}

ArcPlayStoreSearchResult::~ArcPlayStoreSearchResult() = default;

void ArcPlayStoreSearchResult::Open(int event_flags) {
  LaunchIntent(install_intent_uri().value(),
               list_controller_->GetAppListDisplayId());
}

ash::SearchResultType ArcPlayStoreSearchResult::GetSearchResultType() const {
  return is_instant_app() ? ash::PLAY_STORE_INSTANT_APP
                          : ash::PLAY_STORE_UNINSTALLED_APP;
}

void ArcPlayStoreSearchResult::GetContextMenuModel(
    GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<ArcPlayStoreAppContextMenu>(
      this, profile_, list_controller_);
  // TODO(755701): Enable context menu once Play Store API starts returning both
  // install and launch intents.
  std::move(callback).Run(nullptr);
}

void ArcPlayStoreSearchResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

AppContextMenu* ArcPlayStoreSearchResult::GetAppContextMenu() {
  return context_menu_.get();
}

}  // namespace app_list
