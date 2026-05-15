// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ICON_TABLE_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ICON_TABLE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browser_apis/ui_controllers/toolbar/icon_handle.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace toolbar_ui_api {
class IconTableFetcher;
}  // namespace toolbar_ui_api

namespace ui {
class ColorProvider;
class ImageModel;
}  // namespace ui

namespace webui_toolbar {

/**
 * This manages registration of ui::ImageModel's and other kinds of icons
 * to IconHandle to be sent over mojo, as well as computation of corresponding
 * vectors of IconUpdate to keep the other end informed about meanings of these
 * handles.
 */
class IconTable {
 public:
  class Delegate {
   public:
    virtual const ui::ColorProvider* GetColorProvider() const = 0;
    virtual float GetScaleFactor() const = 0;
  };

  // `delegate` must outlast this.
  explicit IconTable(Delegate* delegate);
  IconTable(const IconTable&) = delete;
  IconTable(IconTable&&) = delete;
  ~IconTable();

  IconTable& operator=(const IconTable&) = delete;
  IconTable& operator=(IconTable&&) = delete;

  // Returns an IconHandle to a VectorIcon this class knows about
  // (see KnownIcons() in .cc). Returns a null IconHandle if it doesn't
  // recognize it.
  //
  // It's the WebUI side's responsibility to set the appropriate pen color.
  toolbar_ui_api::IconHandle RegisterVectorIcon(const gfx::VectorIcon& icon);

  // Register `icon` with an IconHandle. If it's not a recognized vector icon
  // it will be rasterized when needed, so this should only be called when
  // the icon changes, as it can be expensive. Note that models with vector
  // icons that could be handled if they were registered will trigger a
  // DHECK failure.
  //
  // Note that for the vector icons, the responsibility is on WebUI side to
  // configure colors.
  toolbar_ui_api::IconHandle RegisterImageModel(ui::ImageModel icon);

  // Creates an IconTableFetcher for `this`. Only one of these should be
  // active at once due to statefullness of the TakePendingUpdates() API.
  std::unique_ptr<toolbar_ui_api::IconTableFetcher> MakeIconTableFetcher();

  // Gets the entire state of the current icon table.
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> GetFullState();

  // Encodes changes to live icons since the last call to
  // TakePendingUpdates(). This may be an over approximation, which is safe
  // since icon IDs are never reused.
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> TakePendingUpdates();

  // Normally trying to rasterize a vector icon rather than mapping it results
  // in a DCHECK. This disables this behavior for tests.
  void PermitFallbackVectorRasterizationForTesting() {
    permit_fallback_vector_rasterization_for_testing_ = true;
  }

 private:
  class ProviderImpl;
  class IconTableFetcherImpl;

  toolbar_ui_api::IconHandle AddRegistration(std::string name_or_url,
                                             bool is_url);

  void UnregisterIcon(toolbar_ui_api::IconHandleId handle_id);

  raw_ptr<Delegate> delegate_;

  // Live icons.
  absl::flat_hash_map<toolbar_ui_api::IconHandleId, raw_ptr<ProviderImpl>>
      registered_icons_;

  // Icons that have changed since last call to TakePendingUpdates().
  absl::flat_hash_set<toolbar_ui_api::IconHandleId> pending_updates_;

  // Icons that possibly depend on scale factor.
  absl::flat_hash_set<toolbar_ui_api::IconHandleId> possibly_scale_dependent_;
  std::optional<float> scale_factor_of_last_update_;

  toolbar_ui_api::IconHandleId::Generator next_id_;

  bool permit_fallback_vector_rasterization_for_testing_ = false;

  base::WeakPtrFactory<IconTable> weak_ptr_factory_{this};
};

}  // namespace webui_toolbar

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ICON_TABLE_H_
