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
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

class SkBitmap;

namespace webui_toolbar {

namespace {

struct IconInfo {
  std::string_view name_or_url;
  bool is_url;
};

const base::flat_map<const gfx::VectorIcon*, IconInfo>& KnownIcons() {
  static base::NoDestructor<base::flat_map<const gfx::VectorIcon*, IconInfo>>
      table({
          {{&vector_icons::kPasswordManagerOldIcon},
           {"rhs_icons/password_manager.svg", true}},
          {{&vector_icons::kLocationOnChromeRefreshOldIcon},
           {"rhs_icons/location_on_chrome_refresh.svg", true}},
          // Used by pinned toolbar actions:
          {{&kIncognitoRefreshMenuOldIcon},
           {"pinned-toolbar-action:NewIncognitoWindow", false}},
          {{&kCreditCardChromeRefreshOldIcon},
           {"pinned-toolbar-action:ShowPaymentsBubbleOrPage", false}},
          {{&kBookmarksSidePanelRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowBookmarks", false}},
          {{&kReadingListOldIcon},
           {"pinned-toolbar-action:SidePanelShowReadingList", false}},
          {{&vector_icons::kHistoryChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowHistoryCluster", false}},
          {{&kDownloadToolbarButtonChromeRefreshOldIcon},
           {"pinned-toolbar-action:ShowDownloads", false}},
          {{&kTrashCanRefreshOldIcon},
           {"pinned-toolbar-action:ClearBrowsingData", false}},
          {{&kPrintMenuOldIcon}, {"pinned-toolbar-action:Print", false}},
          {{&vector_icons::kSearchChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowLensOverlayResults", false}},
          {{&vector_icons::kGTranslateIcon},
           {"pinned-toolbar-action:ShowTranslate", false}},
          {{&kQrCodeChromeRefreshOldIcon},
           {"pinned-toolbar-action:QrCodeGenerator", false}},
          {{&vector_icons::kMediaRouterIdleChromeRefreshOldIcon},
           {"pinned-toolbar-action:RouteMediaIdle", false}},
          {{&vector_icons::kMediaRouterWarningChromeRefreshOldIcon},
           {"pinned-toolbar-action:RouteMediaWarning", false}},
          {{&vector_icons::kMediaRouterPausedOldIcon},
           {"pinned-toolbar-action:RouteMediaPaused", false}},
          {{&vector_icons::kMediaRouterActiveChromeRefreshOldIcon},
           {"pinned-toolbar-action:RouteMediaActive", false}},
          {{&kCastChromeRefreshOldIcon},
           {"pinned-toolbar-action:RouteMedia", false}},
          {{&kMenuBookChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowReadAnything", false}},
          {{&kLinkChromeRefreshOldIcon},
           {"pinned-toolbar-action:CopyUrl", false}},
          {{&kDevicesChromeRefreshOldIcon},
           {"pinned-toolbar-action:SendTabToSelf", false}},
          {{&kTaskManagerOldIcon},
           {"pinned-toolbar-action:TaskManager", false}},
          {{&kDeveloperToolsOldIcon},
           {"pinned-toolbar-action:DevTools", false}},
          {{&kTabSearchTabStripOldIcon},
           {"pinned-toolbar-action:TabSearch", false}},
          {{&kDockToRightSparkIcon},
           {"pinned-toolbar-action:SidePanelShowContextualTasks", false}},
          {{&vector_icons::kImageSearchOldIcon},
           {"pinned-toolbar-action:SidePanelShowLens", false}},
          {{&views::kInfoChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowAboutThisSite", false}},
          {{&vector_icons::kEditChromeRefreshOldIcon},
           {"pinned-toolbar-action:SidePanelShowCustomizeChrome", false}},
          {{&vector_icons::kShoppingBagOldIcon},
           {"pinned-toolbar-action:SidePanelShowShoppingInsights", false}},
          {{&vector_icons::kStorefrontOldIcon},
           {"pinned-toolbar-action:SidePanelShowMerchantTrust", false}},
          {{&vector_icons::kFeedbackOldIcon},
           {"pinned-toolbar-action:SendSharedTabGroupFeedback", false}},
          {{&vector_icons::kChatOldIcon},
           {"pinned-toolbar-action:SidePanelShowComments", false}},

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          {{&vector_icons::kGoogleLensMonochromeLogoIcon},
           {"internal-icons:google_lens_monochrome_logo", false}},
          {{&vector_icons::kPageInsightsIcon},
           {"internal-icons:page_insights", false}},
#endif

      });
  return *table;
}

}  // namespace

class IconTable::ProviderImpl : public toolbar_ui_api::IconHandle::Provider {
 public:
  ProviderImpl(IconTable* icon_table,
               toolbar_ui_api::IconHandleId handle_id,
               std::string name_or_url,
               bool is_url)
      : icon_table_(icon_table),
        handle_id_(handle_id),
        name_or_url_(std::move(name_or_url)),
        is_url_(is_url) {}

  ProviderImpl(IconTable* icon_table,
               toolbar_ui_api::IconHandleId handle_id,
               ui::ImageModel image_model)
      : icon_table_(icon_table),
        handle_id_(handle_id),
        is_url_(true),  // Will produce a URL.
        image_model_(std::move(image_model)) {}

  toolbar_ui_api::IconHandleId HandleId() override { return handle_id_; }

  void Detach() { icon_table_ = nullptr; }

  toolbar_ui_api::mojom::IconUpdatePtr ToMojom(float scale_factor) {
    if (image_model_) {
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
                                                  name_or_url_, is_url_);
  }

 private:
  ~ProviderImpl() override {
    if (icon_table_) {
      icon_table_->UnregisterIcon(handle_id_);
    }
  }

  raw_ptr<IconTable> icon_table_;
  const toolbar_ui_api::IconHandleId handle_id_;
  std::string name_or_url_;
  bool is_url_;

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
    const gfx::VectorIcon& icon) {
  const auto& known_icons = KnownIcons();
  auto it = known_icons.find(&icon);
  if (it == known_icons.end()) {
    return toolbar_ui_api::IconHandle();
  }

  return AddRegistration(std::string(it->second.name_or_url),
                         it->second.is_url);
}

toolbar_ui_api::IconHandle IconTable::RegisterImageModel(ui::ImageModel icon) {
  if (icon.IsEmpty()) {
    return toolbar_ui_api::IconHandle();
  }

  if (icon.IsVectorIcon()) {
    const auto& vector_icon_model = icon.GetVectorIcon();
    if (vector_icon_model.vector_icon() && !vector_icon_model.badge_icon()) {
      const gfx::VectorIcon& vector_icon = *icon.GetVectorIcon().vector_icon();
      toolbar_ui_api::IconHandle maybe_icon = RegisterVectorIcon(vector_icon);
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

  auto handle_id = next_id_.GenerateNextId();
  DCHECK(handle_id.value() != toolbar_ui_api::kNullIconHandleId);
  auto provider_impl =
      base::MakeRefCounted<ProviderImpl>(this, handle_id, std::move(icon));
  registered_icons_.insert(std::pair(handle_id, provider_impl.get()));
  pending_updates_.insert(handle_id);
  possibly_scale_dependent_.insert(handle_id);
  return toolbar_ui_api::IconHandle(std::move(provider_impl));
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
          id.value(), std::nullopt, /*is_url=*/false));
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

toolbar_ui_api::IconHandle IconTable::AddRegistration(std::string name_or_url,
                                                      bool is_url) {
  auto handle_id = next_id_.GenerateNextId();
  DCHECK(handle_id.value() != toolbar_ui_api::kNullIconHandleId);
  auto provider_impl = base::MakeRefCounted<ProviderImpl>(
      this, handle_id, std::move(name_or_url), is_url);
  registered_icons_.insert(std::pair(handle_id, provider_impl.get()));
  pending_updates_.insert(handle_id);
  return toolbar_ui_api::IconHandle(provider_impl);
}

void IconTable::UnregisterIcon(toolbar_ui_api::IconHandleId handle_id) {
  DCHECK(handle_id.value() != toolbar_ui_api::kNullIconHandleId);
  pending_updates_.insert(handle_id);
  registered_icons_.erase(handle_id);
  possibly_scale_dependent_.erase(handle_id);
}

}  // namespace webui_toolbar
