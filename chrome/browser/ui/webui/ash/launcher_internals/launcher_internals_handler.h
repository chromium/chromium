// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LAUNCHER_INTERNALS_LAUNCHER_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LAUNCHER_INTERNALS_LAUNCHER_INTERNALS_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

class ChromeSearchResult;

namespace ash {

class LauncherInternalsHandler : public app_list::SearchController::Observer {
 public:
  LauncherInternalsHandler(
      app_list::SearchController* search_controller,
      mojo::PendingRemote<launcher_internals::mojom::Page> page);
  ~LauncherInternalsHandler() override;

  LauncherInternalsHandler(const LauncherInternalsHandler&) = delete;
  LauncherInternalsHandler& operator=(const LauncherInternalsHandler&) = delete;

  // app_list::SearchController::Observer:
  void OnResultsAdded(
      const std::u16string& query,
      const std::vector<app_list::KeywordInfo>& extracted_keyword_info,
      const std::vector<const ChromeSearchResult*>& results) override;

 private:
  mojo::Remote<launcher_internals::mojom::Page> page_;

  base::ScopedObservation<app_list::SearchController,
                          app_list::SearchController::Observer>
      search_controller_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LAUNCHER_INTERNALS_LAUNCHER_INTERNALS_HANDLER_H_
