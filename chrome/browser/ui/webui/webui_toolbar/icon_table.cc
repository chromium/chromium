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

const base::flat_map<const gfx::VectorIcon*, IconInfo>& KnownIcons() {
  static base::NoDestructor<base::flat_map<const gfx::VectorIcon*, IconInfo>>
      table({
          {{&vector_icons::kPasswordManagerOldIcon},
           {"rhs_icons/password_manager.svg", IconType::kMaskUrl}},
          {{&vector_icons::kLocationOnChromeRefreshOldIcon},
           {"rhs_icons/location_on_chrome_refresh.svg", IconType::kMaskUrl}},

          // LHS icons:
          {{&vector_icons::kDangerousChromeRefreshOldIcon},
           {"lhs_icons/dangerous_chrome_refresh.svg", IconType::kMaskUrl}},
          {{&vector_icons::kNotSecureWarningChromeRefreshOldIcon},
           {"lhs_icons/not_secure_warning_chrome_refresh_16.svg",
            IconType::kMaskUrl}},
          {{&omnibox::kHttpChromeRefreshOldIcon},
           {"lhs_icons/http_chrome_refresh.svg", IconType::kMaskUrl}},
          {{&omnibox::kPageChromeRefreshOldIcon},
           {"lhs_icons/page_chrome_refresh_icon.svg", IconType::kMaskUrl}},
          {{&omnibox::kProductChromeRefreshOldIcon},
           {"lhs_icons/product_chrome_refresh_icon.svg", IconType::kMaskUrl}},
          {{&omnibox::kSecurePageInfoChromeRefreshOldIcon},
           {"lhs_icons/secure_page_info_chrome_refresh.svg",
            IconType::kMaskUrl}},

          // Used by pinned toolbar actions:
          {{&kIncognitoRefreshMenuOldIcon},
           {"pinned-toolbar-action:NewIncognitoWindow", IconType::kIconSet}},
          {{&kCreditCardChromeRefreshOldIcon},
           {"pinned-toolbar-action:ShowPaymentsBubbleOrPage",
            IconType::kIconSet}},
          {{&kBookmarksSidePanelRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowBookmarks",
            IconType::kIconSet}},
          {{&kReadingListOldIcon},
           {"pinned-toolbar-action:SidePanelShowReadingList",
            IconType::kIconSet}},
          {{&vector_icons::kHistoryChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowHistoryCluster",
            IconType::kIconSet}},
          {{&kDownloadToolbarButtonChromeRefreshOldIcon},
           {"pinned-toolbar-action:ShowDownloads", IconType::kIconSet}},
          {{&kTrashCanRefreshOldIcon},
           {"pinned-toolbar-action:ClearBrowsingData", IconType::kIconSet}},
          {{&kPrintMenuOldIcon},
           {"pinned-toolbar-action:Print", IconType::kIconSet}},
          {{&vector_icons::kSearchChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowLensOverlayResults",
            IconType::kIconSet}},
          {{&vector_icons::kGTranslateIcon},
           {"pinned-toolbar-action:ShowTranslate", IconType::kIconSet}},
          {{&kQrCodeChromeRefreshOldIcon},
           {"pinned-toolbar-action:QrCodeGenerator", IconType::kIconSet}},
          {{&vector_icons::kMediaRouterIdleChromeRefreshOldIcon},
           {"pinned-toolbar-action:RouteMediaIdle", IconType::kIconSet}},
          {{&vector_icons::kMediaRouterWarningChromeRefreshOldIcon},
           {"pinned-toolbar-action:RouteMediaWarning", IconType::kIconSet}},
          {{&vector_icons::kMediaRouterPausedOldIcon},
           {"pinned-toolbar-action:RouteMediaPaused", IconType::kIconSet}},
          {{&vector_icons::kMediaRouterActiveChromeRefreshOldIcon},
           {"pinned-toolbar-action:RouteMediaActive", IconType::kIconSet}},
          {{&kCastChromeRefreshOldIcon},
           {"pinned-toolbar-action:RouteMedia", IconType::kIconSet}},
          {{&kMenuBookChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowReadAnything",
            IconType::kIconSet}},
          {{&kLinkChromeRefreshOldIcon},
           {"pinned-toolbar-action:CopyUrl", IconType::kIconSet}},
          {{&kDevicesChromeRefreshOldIcon},
           {"pinned-toolbar-action:SendTabToSelf", IconType::kIconSet}},
          {{&kTaskManagerOldIcon},
           {"pinned-toolbar-action:TaskManager", IconType::kIconSet}},
          {{&kDeveloperToolsOldIcon},
           {"pinned-toolbar-action:DevTools", IconType::kIconSet}},
          {{&kTabSearchTabStripOldIcon},
           {"pinned-toolbar-action:TabSearch", IconType::kIconSet}},
          {{&kDockToRightSparkIcon},
           {"pinned-toolbar-action:SidePanelShowContextualTasks",
            IconType::kIconSet}},
          {{&vector_icons::kImageSearchOldIcon},
           {"pinned-toolbar-action:SidePanelShowLens", IconType::kIconSet}},
          {{&views::kInfoChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowAboutThisSite",
            IconType::kIconSet}},
          {{&vector_icons::kEditChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowCustomizeChrome",
            IconType::kIconSet}},
          {{&vector_icons::kShoppingBagOldIcon},
           {"pinned-toolbar-action:SidePanelShowShoppingInsights",
            IconType::kIconSet}},
          {{&vector_icons::kStorefrontOldIcon},
           {"pinned-toolbar-action:SidePanelShowMerchantTrust",
            IconType::kIconSet}},
          {{&vector_icons::kFeedbackOldIcon},
           {"pinned-toolbar-action:SendSharedTabGroupFeedback",
            IconType::kIconSet}},
          {{&vector_icons::kChatOldIcon},
           {"pinned-toolbar-action:SidePanelShowComments", IconType::kIconSet}},

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          {{&vector_icons::kGoogleLensMonochromeLogoIcon},
           {"internal-icons:google_lens_monochrome_logo", IconType::kIconSet}},
          {{&vector_icons::kPageInsightsIcon},
           {"internal-icons:page_insights", IconType::kIconSet}},
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
    if (need_rasterize_) {
      // The downside of lazy rasterization like this is that a lot may
      // happen at once; but it will also not be done until it's needed and
      // it triggers scale factor changes transparently.
      if (rasterized_scale_ != scale_factor) {
        name_or_url_ = webui::EncodePNGAndMakeDataURI(
            image_model_->Rasterize(icon_table_->delegate_->GetColorProvider()),
            scale_factor);
      }
    }

    return toolbar_ui_api::mojom::IconUpdate::New(handle_id_.value(),
                                                  name_or_url_, icon_type_);
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
  std::optional<float> rasterized_scale_ = std::nullopt;
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

toolbar_ui_api::IconHandle IconTable::RegisterVectorIcon(
    const gfx::VectorIcon& icon,
    std::optional<ui::ImageModel> model_info) {
  const auto& known_icons = KnownIcons();
  auto it = known_icons.find(&icon);
  if (it == known_icons.end()) {
    return toolbar_ui_api::IconHandle();
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
      const gfx::VectorIcon& vector_icon = *icon.GetVectorIcon().vector_icon();
      toolbar_ui_api::IconHandle maybe_icon =
          RegisterVectorIcon(vector_icon, icon);
      if (!maybe_icon.is_null()) {
        return maybe_icon;
      } else {
        // If this hit, please add a WebUI version of the icon
        // (via iconset or SVG to chrome/browser/resources/webui_toolbar/ if
        // it's not already there, and add a mapping above in KnownIcons().
        // TODO(crbug.com/511760342): probably want to DWoC here when more
        // mature.
        DCHECK(permit_fallback_vector_rasterization_for_testing_)
            << "Don't know how to map:" << vector_icon.name;
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
          id.value(), std::nullopt, IconType::kMaskUrl));
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
