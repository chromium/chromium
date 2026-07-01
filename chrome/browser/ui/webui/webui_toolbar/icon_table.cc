// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/icon_table.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/icon_table_fetcher.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

class SkBitmap;

using toolbar_ui_api::mojom::IconType;

namespace webui_toolbar {

namespace {

struct IconInfo {
  std::string_view name_or_url;
  IconType type;
};

// Mapping from vector_icon objects to info on how to render them on the
// WebUI end. Note that the VectorIcon is the /key/.
const base::flat_map<const gfx::VectorIcon*, IconInfo>& KnownIcons() {
  static base::NoDestructor<base::flat_map<const gfx::VectorIcon*, IconInfo>>
      table({
          {{&kAccountBoxIcon},
           {"webui-toolbar:account_box", IconType::kIconSet}},
          {{&kAccountBoxOldIcon},
           {"webui-toolbar:account_box", IconType::kIconSet}},
          {{&kBookmarksSidePanelRefreshOldIcon},
           {"webui-toolbar:hotel_class", IconType::kIconSet}},
          {{&kCastChromeRefreshOldIcon},
           {"webui-toolbar:cast", IconType::kIconSet}},
          {{&kCastIcon}, {"webui-toolbar:cast", IconType::kIconSet}},
          {{&kCodeIcon}, {"webui-toolbar:code", IconType::kIconSet}},
          {{&kCreditCardChromeRefreshOldIcon},
           {"webui-toolbar:credit_card", IconType::kIconSet}},
          {{&kCreditCardIcon},
           {"webui-toolbar:credit_card", IconType::kIconSet}},
          {{&kDeleteIcon}, {"webui-toolbar:delete", IconType::kIconSet}},
          {{&kDeveloperToolsOldIcon},
           {"webui-toolbar:code", IconType::kIconSet}},
          {{&kDevicesChromeRefreshOldIcon},
           {"webui-toolbar:devices", IconType::kIconSet}},
          {{&kDevicesIcon}, {"webui-toolbar:devices", IconType::kIconSet}},
          {{&kDockToRightSparkCustomIcon},
           {"webui-toolbar:dock_to_right_spark_custom", IconType::kIconSet}},
          {{&kDownloadIcon}, {"webui-toolbar:download", IconType::kIconSet}},
          {{&kDownloadToolbarButtonChromeRefreshOldIcon},
           {"webui-toolbar:download", IconType::kIconSet}},
          {{&kEditIcon}, {"webui-toolbar:edit", IconType::kIconSet}},
          {{&kGTranslateIcon},
           {"webui-toolbar:g_translate", IconType::kIconSet}},
          {{&kHotelClassIcon},
           {"webui-toolbar:hotel_class", IconType::kIconSet}},
          {{&kIncognitoIcon}, {"webui-toolbar:incognito", IconType::kIconSet}},
          {{&kIncognitoRefreshMenuOldIcon},
           {"webui-toolbar:incognito", IconType::kIconSet}},
          {{&kInfoIcon}, {"webui-toolbar:info", IconType::kIconSet}},
          {{&kLinkChromeRefreshOldIcon},
           {"webui-toolbar:link", IconType::kIconSet}},
          {{&kLinkIcon}, {"webui-toolbar:link", IconType::kIconSet}},
          {{&kListAltIcon}, {"webui-toolbar:list_alt", IconType::kIconSet}},
          {{&kManageSearchIcon},
           {"webui-toolbar:manage_search", IconType::kIconSet}},
          {{&kMenuBookChromeRefreshOldIcon},
           {"webui-toolbar:menu_book", IconType::kIconSet}},
          {{&kMenuBookIcon}, {"webui-toolbar:menu_book", IconType::kIconSet}},
          {{&kPrintIcon}, {"webui-toolbar:print", IconType::kIconSet}},
          {{&kPrintMenuOldIcon}, {"webui-toolbar:print", IconType::kIconSet}},
          {{&kQrCodeChromeRefreshOldIcon},
           {"webui-toolbar:qr_code", IconType::kIconSet}},
          {{&kQrCodeIcon}, {"webui-toolbar:qr_code", IconType::kIconSet}},
          {{&kReadingListOldIcon},
           {"webui-toolbar:list_alt", IconType::kIconSet}},
          {{&kTabSearchTabStripOldIcon},
           {"webui-toolbar:manage_search", IconType::kIconSet}},
          {{&kTableChartIcon},
           {"webui-toolbar:table_chart", IconType::kIconSet}},
          {{&kTaskManagerOldIcon},
           {"webui-toolbar:table_chart", IconType::kIconSet}},
          {{&kTrashCanRefreshOldIcon},
           {"webui-toolbar:delete", IconType::kIconSet}},
          {{&omnibox::kBookmarkChromeRefreshOldIcon},
           {"webui-toolbar:star", IconType::kIconSet}},
          {{&omnibox::kChromeProductIcon},
           {"webui-toolbar:chrome_product", IconType::kIconSet}},
          {{&omnibox::kHttpChromeRefreshOldIcon},
           {"webui-toolbar:info", IconType::kIconSet}},
          {{&omnibox::kInfoIcon}, {"webui-toolbar:info", IconType::kIconSet}},
          {{&omnibox::kPageChromeRefreshOldIcon},
           {"webui-toolbar:public", IconType::kIconSet}},
          {{&omnibox::kPageInfoCustomIcon},
           {"webui-toolbar:page_info_custom", IconType::kIconSet}},
          {{&omnibox::kProductChromeRefreshOldIcon},
           {"webui-toolbar:chrome_product", IconType::kIconSet}},
          {{&omnibox::kPublicIcon},
           {"webui-toolbar:public", IconType::kIconSet}},
          {{&omnibox::kSearchSparkIcon},
           {"webui-toolbar:search_spark", IconType::kIconSet}},
          {{&omnibox::kSearchSparkOldIcon},
           {"webui-toolbar:search_spark", IconType::kIconSet}},
          {{&omnibox::kSecurePageInfoChromeRefreshOldIcon},
           {"webui-toolbar:page_info_custom", IconType::kIconSet}},
          {{&omnibox::kStarActiveChromeRefreshOldIcon},
           {"webui-toolbar:star_filled", IconType::kIconSet}},
          {{&omnibox::kStarFilledIcon},
           {"webui-toolbar:star_filled", IconType::kIconSet}},
          {{&omnibox::kStarIcon}, {"webui-toolbar:star", IconType::kIconSet}},
          {{&vector_icons::kBusinessChromeRefreshOldIcon},
           {"webui-toolbar:domain", IconType::kIconSet}},
          {{&vector_icons::kCastConnectedIcon},
           {"webui-toolbar:cast_connected", IconType::kIconSet}},
          {{&vector_icons::kCastIcon},
           {"webui-toolbar:cast", IconType::kIconSet}},
          {{&vector_icons::kCastPauseIcon},
           {"webui-toolbar:cast_pause", IconType::kIconSet}},
          {{&vector_icons::kCastWarningIcon},
           {"webui-toolbar:cast_warning", IconType::kIconSet}},
          {{&vector_icons::kChatIcon},
           {"webui-toolbar:chat", IconType::kIconSet}},
          {{&vector_icons::kChatOldIcon},
           {"webui-toolbar:chat", IconType::kIconSet}},
          {{&vector_icons::kDangerousChromeRefreshOldIcon},
           {"webui-toolbar:dangerous_filled", IconType::kIconSet}},
          {{&vector_icons::kDangerousFilledIcon},
           {"webui-toolbar:dangerous_filled", IconType::kIconSet}},
          {{&vector_icons::kDomainIcon},
           {"webui-toolbar:domain", IconType::kIconSet}},
          {{&vector_icons::kEditChromeRefreshOldIcon},
           {"webui-toolbar:edit", IconType::kIconSet}},
          {{&vector_icons::kEditIcon},
           {"webui-toolbar:edit", IconType::kIconSet}},
          {{&vector_icons::kFeedbackIcon},
           {"webui-toolbar:feedback", IconType::kIconSet}},
          {{&vector_icons::kFeedbackOldIcon},
           {"webui-toolbar:feedback", IconType::kIconSet}},
          {{&vector_icons::kGTranslateIcon},
           {"webui-toolbar:g_translate", IconType::kIconSet}},
          {{&vector_icons::kHistoryChromeRefreshOldIcon},
           {"webui-toolbar:history", IconType::kIconSet}},
          {{&vector_icons::kHistoryIcon},
           {"webui-toolbar:history", IconType::kIconSet}},
          {{&vector_icons::kImageSearchIcon},
           {"webui-toolbar:image_search", IconType::kIconSet}},
          {{&vector_icons::kImageSearchOldIcon},
           {"webui-toolbar:image_search", IconType::kIconSet}},
          {{&vector_icons::kLocationOnChromeRefreshOldIcon},
           {"webui-toolbar:location_on", IconType::kIconSet}},
          {{&vector_icons::kLocationOnIcon},
           {"webui-toolbar:location_on", IconType::kIconSet}},
          {{&vector_icons::kMediaRouterActiveChromeRefreshOldIcon},
           {"webui-toolbar:cast_connected", IconType::kIconSet}},
          {{&vector_icons::kMediaRouterIdleChromeRefreshOldIcon},
           {"webui-toolbar:cast", IconType::kIconSet}},
          {{&vector_icons::kMediaRouterPausedOldIcon},
           {"webui-toolbar:cast_pause", IconType::kIconSet}},
          {{&vector_icons::kMediaRouterWarningChromeRefreshOldIcon},
           {"webui-toolbar:cast_warning", IconType::kIconSet}},
          {{&vector_icons::kNoEncryptionIcon},
           {"webui-toolbar:no_encryption", IconType::kIconSet}},
          {{&vector_icons::kNoEncryptionOldIcon},
           {"webui-toolbar:no_encryption", IconType::kIconSet}},
          {{&vector_icons::kNotSecureWarningChromeRefreshOldIcon},
           {"webui-toolbar:warning", IconType::kIconSet}},
          {{&vector_icons::kPasswordManagerIcon},
           {"webui-toolbar:password_manager", IconType::kIconSet}},
          {{&vector_icons::kPasswordManagerOldIcon},
           {"webui-toolbar:password_manager", IconType::kIconSet}},
          {{&vector_icons::kSearchChromeRefreshOldIcon},
           {"webui-toolbar:search", IconType::kIconSet}},
          {{&vector_icons::kSearchIcon},
           {"webui-toolbar:search", IconType::kIconSet}},
          {{&vector_icons::kSearchOldIcon},
           {"webui-toolbar:search_old", IconType::kIconSet}},
          {{&vector_icons::kShoppingBagIcon},
           {"webui-toolbar:shopping_bag", IconType::kIconSet}},
          {{&vector_icons::kShoppingBagOldIcon},
           {"webui-toolbar:shopping_bag", IconType::kIconSet}},
          {{&vector_icons::kStorefrontIcon},
           {"webui-toolbar:storefront", IconType::kIconSet}},
          {{&vector_icons::kStorefrontOldIcon},
           {"webui-toolbar:storefront", IconType::kIconSet}},
          {{&vector_icons::kWarningIcon},
           {"webui-toolbar:warning", IconType::kIconSet}},
          {{&views::kInfoChromeRefreshOldIcon},
           {"webui-toolbar:info", IconType::kIconSet}},
          {{&views::kInfoIcon}, {"webui-toolbar:info", IconType::kIconSet}},

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          {{&vector_icons::kGoogleLensMonochromeLogoIcon},
           {"internal-icons:google_lens_monochrome_logo", IconType::kIconSet}},
          {{&vector_icons::kPageInsightsIcon},
           {"internal-icons:page_insights", IconType::kIconSet}},
          {{&vector_icons::kGoogleGLogoMonochromeIcon},
           {"internal-icons:google_g_logo_monochrome", IconType::kIconSet}},
#endif

      });
  return *table;
}

}  // namespace

class IconTable::ProviderImpl : public toolbar_ui_api::IconHandle::Provider {
 public:
  // If `need_rasterize` is true, `image_model` will be used for rendering;
  // otherwise the provided value of `name_or_url` will be used.
  // The image model is also used for reuse checks.
  ProviderImpl(IconTable* icon_table,
               toolbar_ui_api::IconHandleId handle_id,
               bool need_rasterize,
               std::string name_or_url,
               IconType icon_type,
               std::optional<ui::ImageModel> image_model)
      : icon_table_(icon_table),
        handle_id_(handle_id),
        need_rasterize_(need_rasterize),
        name_or_url_(std::move(name_or_url)),
        icon_type_(icon_type),
        image_model_(image_model) {
    if (need_rasterize) {
      DCHECK(image_model_.has_value());
    }
  }

  toolbar_ui_api::IconHandleId HandleId() override { return handle_id_; }

  void Detach() { icon_table_ = nullptr; }

  toolbar_ui_api::mojom::IconUpdatePtr ToMojom(float scale_factor) {
    std::optional<SkColor> color;
    if (need_rasterize_) {
      // The downside of lazy rasterization like this is that a lot may
      // happen at once; but it will also not be done until it's needed and
      // it triggers scale factor changes transparently.
      if (rasterized_scale_ != scale_factor) {
        name_or_url_ = webui::EncodePNGAndMakeDataURI(
            image_model_->Rasterize(icon_table_->delegate_->GetColorProvider()),
            scale_factor);
      }
    } else if (image_model_ && image_model_->IsVectorIcon()) {
      ui::ColorVariant color_variant = image_model_->GetVectorIcon().color();
      color = color_variant.ResolveToSkColor(
          icon_table_->delegate_->GetColorProvider());
    }
    return toolbar_ui_api::mojom::IconUpdate::New(
        handle_id_.value(), name_or_url_, icon_type_, color);
  }

  const std::optional<ui::ImageModel>& MaybeImageModel() {
    return image_model_;
  }

 private:
  ~ProviderImpl() override {
    if (icon_table_) {
      icon_table_->UnregisterIcon(handle_id_);
    }
  }

  raw_ptr<IconTable> icon_table_;
  const toolbar_ui_api::IconHandleId handle_id_;
  const bool need_rasterize_;
  std::string name_or_url_;
  const IconType icon_type_;

  std::optional<ui::ImageModel> image_model_;
  // Set if `image_model_` got rendered to `name_or_url_`.
  std::optional<float> rasterized_scale_;
};

class IconTable::IconTableFetcherImpl
    : public toolbar_ui_api::IconTableFetcher {
 public:
  explicit IconTableFetcherImpl(base::WeakPtr<IconTable> icon_table)
      : icon_table_(icon_table) {}

  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> GetFullState() override {
    if (!icon_table_) {
      return {};
    }
    return icon_table_->GetFullState();
  }

  // Gets changes since the last time TakePendingUpdates() was called.
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> TakePendingUpdates()
      override {
    if (!icon_table_) {
      return {};
    }
    return icon_table_->TakePendingUpdates();
  }

 private:
  base::WeakPtr<IconTable> icon_table_;
};

IconTable::IconTable(Delegate* delegate) : delegate_(delegate) {}

IconTable::~IconTable() {
  for (auto& kv : registered_icons_) {
    kv.second->Detach();
  }
  registered_icons_.clear();
}

std::optional<toolbar_ui_api::IconHandle> IconTable::RegisterVectorIcon(
    const gfx::VectorIcon& icon,
    std::optional<ui::ImageModel> model_info) {
  const auto& known_icons = KnownIcons();
  auto it = known_icons.find(&icon);
  if (it == known_icons.end()) {
    return &icon == &gfx::VectorIcon::EmptyIcon()
               ? std::optional(toolbar_ui_api::IconHandle())
               : std::nullopt;
  }

  return AddRegistration(/*need_rasterize=*/false,
                         std::string(it->second.name_or_url), it->second.type,
                         std::move(model_info));
}

toolbar_ui_api::IconHandle IconTable::RegisterImageModel(ui::ImageModel icon) {
  if (icon.IsEmpty()) {
    return toolbar_ui_api::IconHandle();
  }

  if (icon.IsVectorIcon()) {
    const auto& vector_icon_model = icon.GetVectorIcon();
    if (vector_icon_model.vector_icon() && !vector_icon_model.badge_icon()) {
      const gfx::VectorIcon& vector_icon = *vector_icon_model.vector_icon();
      std::optional<toolbar_ui_api::IconHandle> maybe_icon =
          RegisterVectorIcon(vector_icon, icon);
      if (maybe_icon.has_value()) {
        return *maybe_icon;
      } else {
        // If this hit, please add a WebUI version of the icon
        // (via iconset or SVG to chrome/browser/resources/webui_toolbar/ if
        // it's not already there, and add a mapping above in KnownIcons().
        // TODO(crbug.com/511760342): probably want to DWoC here when more
        // mature.
        DCHECK(permit_fallback_vector_rasterization_for_testing_)
            << "Don't know how to map:"
            << (vector_icon.name ? std::string_view(vector_icon.name)
                                 : std::string_view("(null name)"));
      }
    }
  }

  return AddRegistration(/*need_rasterize=*/true,
                         /*name_or_url=*/std::string(),
                         /* Will generate a URL to full-color PNG */
                         IconType::kFullColorUrl, std::move(icon));
}

toolbar_ui_api::IconHandle IconTable::RegisterImageModelTryReuse(
    ui::ImageModel icon,
    toolbar_ui_api::IconHandle previous_handle) {
  if (!previous_handle.is_null()) {
    toolbar_ui_api::IconHandleId handle_id = previous_handle.HandleId();
    if (auto it = registered_icons_.find(handle_id);
        it != registered_icons_.end()) {
      const auto& maybe_existing = it->second->MaybeImageModel();
      if (maybe_existing == icon) {
        return previous_handle;
      }
    }
  }
  return RegisterImageModel(std::move(icon));
}

toolbar_ui_api::IconHandle IconTable::RegisterColorUrl(std::string_view url) {
  return AddRegistration(/*need_rasterize=*/false,
                         /*name_or_url=*/std::string(url),
                         IconType::kFullColorUrl,
                         /*image_model=*/std::nullopt);
}

std::unique_ptr<toolbar_ui_api::IconTableFetcher>
IconTable::MakeIconTableFetcher() {
  return std::make_unique<IconTableFetcherImpl>(weak_ptr_factory_.GetWeakPtr());
}

std::vector<toolbar_ui_api::mojom::IconUpdatePtr> IconTable::GetFullState() {
  float scale_factor = delegate_->GetScaleFactor();
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons;
  icons.reserve(registered_icons_.size());
  for (const auto& kv : registered_icons_) {
    icons.push_back(kv.second->ToMojom(scale_factor));
  }
  return icons;
}

std::vector<toolbar_ui_api::mojom::IconUpdatePtr>
IconTable::TakePendingUpdates() {
  float scale_factor = delegate_->GetScaleFactor();
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> updates;
  updates.reserve(pending_updates_.size());
  for (auto id : pending_updates_) {
    if (auto it = registered_icons_.find(id); it != registered_icons_.end()) {
      updates.push_back(it->second->ToMojom(scale_factor));
    } else {
      // The icon got deleted on the C++ end since last update; let the JS end
      // know it can free up memory, too.
      updates.push_back(toolbar_ui_api::mojom::IconUpdate::New(
          id.value(), std::nullopt, IconType::kMaskUrl,
          /*color=*/std::nullopt));
    }
  }

  if (scale_factor != scale_factor_of_last_update_) {
    for (auto id : possibly_scale_dependent_) {
      if (pending_updates_.find(id) != pending_updates_.end()) {
        continue;
      }
      auto it = registered_icons_.find(id);
      CHECK(it != registered_icons_.end());
      updates.push_back(it->second->ToMojom(scale_factor));
    }
  }
  scale_factor_of_last_update_ = scale_factor;

  pending_updates_.clear();
  return updates;
}

toolbar_ui_api::IconHandle IconTable::AddRegistration(
    bool need_rasterize,
    std::string name_or_url,
    IconType icon_type,
    std::optional<ui::ImageModel> image_model) {
  auto handle_id = next_id_.GenerateNextId();
  DCHECK(handle_id.value() != toolbar_ui_api::kNullIconHandleId);
  auto provider_impl = base::MakeRefCounted<ProviderImpl>(
      this, handle_id, need_rasterize, std::move(name_or_url), icon_type,
      std::move(image_model));
  registered_icons_.insert(std::pair(handle_id, provider_impl.get()));
  pending_updates_.insert(handle_id);
  if (need_rasterize) {
    possibly_scale_dependent_.insert(handle_id);
  }

  return toolbar_ui_api::IconHandle(std::move(provider_impl));
}

void IconTable::UnregisterIcon(toolbar_ui_api::IconHandleId handle_id) {
  DCHECK(handle_id.value() != toolbar_ui_api::kNullIconHandleId);
  pending_updates_.insert(handle_id);
  registered_icons_.erase(handle_id);
  possibly_scale_dependent_.erase(handle_id);
}

}  // namespace webui_toolbar
