// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_DATA_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_DATA_SEARCH_RESULT_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/arc/mojom/app.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AppListControllerDelegate;

namespace arc {
class IconDecodeRequest;
}  // namespace arc

namespace app_list {

class ArcAppDataSearchResult : public ChromeSearchResult {
 public:
  ArcAppDataSearchResult(arc::mojom::AppDataResultPtr data,
                         AppListControllerDelegate* list_controller,
                         const std::u16string& query);
  ~ArcAppDataSearchResult() override;

  // ChromeSearchResult:
  void GetContextMenuModel(GetMenuModelCallback callback) override;
  void Open(int event_flags) override;

 private:
  const std::string& launch_intent_uri() const {
    return data_->launch_intent_uri;
  }
  const absl::optional<std::vector<uint8_t>>& icon_png_data() const {
    // TODO(crbug.com/1083331): Remove the checking, when the ARC change is
    // rolled in Chrome OS.
    if (!data_->icon || !data_->icon->icon_png_data ||
        data_->icon->icon_png_data->empty()) {
      return data_->icon_png_data;
    }
    return data_->icon->icon_png_data;
  }

  // Set |icon| to SearchResult. |icon| may be customized based on |data_|.
  void ApplyIcon(const gfx::ImageSkia& icon);

  arc::mojom::AppDataResultPtr data_;
  std::unique_ptr<arc::IconDecodeRequest> icon_decode_request_;

  AppListControllerDelegate* const list_controller_;  // Owned by AppListClient.

  base::WeakPtrFactory<ArcAppDataSearchResult> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcAppDataSearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_DATA_SEARCH_RESULT_H_
