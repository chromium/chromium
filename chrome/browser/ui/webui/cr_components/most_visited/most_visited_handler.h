// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_MOST_VISITED_MOST_VISITED_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_MOST_VISITED_MOST_VISITED_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/section_type.h"
#include "content/public/browser/prerender_handle.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

// Handles bidirectional communication between MV tiles and the browser.
class MostVisitedHandler : public most_visited::mojom::MostVisitedPageHandler,
                           public ntp_tiles::MostVisitedSites::Observer,
                           public web_app::PreinstalledWebAppManager::Observer {
 public:
  MostVisitedHandler(
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
          pending_page_handler,
      mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& ntp_url,
      const base::Time& ntp_navigation_start_time);
  MostVisitedHandler(const MostVisitedHandler&) = delete;
  MostVisitedHandler& operator=(const MostVisitedHandler&) = delete;
  ~MostVisitedHandler() override;

  // See MostVisitedSites::EnableCustomLinks.
  void EnableCustomLinks(bool enable);
  // See MostVisitedSites::SetShortcutsVisible.
  void SetShortcutsVisible(bool visible);

  // most_visited::mojom::MostVisitedPageHandler:
  void AddMostVisitedTile(const GURL& url,
                          const std::string& title,
                          AddMostVisitedTileCallback callback) override;
  void DeleteMostVisitedTile(const GURL& url) override;
  void RestoreMostVisitedDefaults() override;
  void ReorderMostVisitedTile(const GURL& url, uint8_t new_pos) override;
  void UndoMostVisitedTileAction() override;
  void UpdateMostVisitedInfo() override;
  void UpdateMostVisitedTile(const GURL& url,
                             const GURL& new_url,
                             const std::string& new_title,
                             UpdateMostVisitedTileCallback callback) override;
  void PrerenderMostVisitedTile(most_visited::mojom::MostVisitedTilePtr tile,
                                bool is_hover_trigger) override;
  void PreconnectMostVisitedTile(
      most_visited::mojom::MostVisitedTilePtr tile) override;
  void CancelPrerender() override;
  void OnMostVisitedTilesRendered(
      std::vector<most_visited::mojom::MostVisitedTilePtr> tiles,
      double time) override;
  void OnMostVisitedTileNavigation(most_visited::mojom::MostVisitedTilePtr tile,
                                   uint32_t index,
                                   uint8_t mouse_button,
                                   bool alt_key,
                                   bool ctrl_key,
                                   bool meta_key,
                                   bool shift_key) override;

 private:
  // ntp_tiles::MostVisitedSites::Observer:
  void OnURLsAvailable(
      const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
          sections) override;
  void OnIconMadeAvailable(const GURL& site_url) override;

  raw_ptr<Profile> profile_;
  // web_app::PreinstalledWebAppManager::Observer
  void OnMigrationRun() override;
  void OnDestroyed() override;

  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites_;
  raw_ptr<content::WebContents> web_contents_;
  NTPUserDataLogger logger_;
  base::Time ntp_navigation_start_time_;
  GURL last_blocklisted_;

  base::WeakPtr<content::PrerenderHandle> prerender_handle_;

  mojo::Receiver<most_visited::mojom::MostVisitedPageHandler> page_handler_;
  mojo::Remote<most_visited::mojom::MostVisitedPage> page_;

  base::ScopedObservation<web_app::PreinstalledWebAppManager,
                          web_app::PreinstalledWebAppManager::Observer>
      preinstalled_web_app_observer_{this};

  base::WeakPtrFactory<MostVisitedHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_MOST_VISITED_MOST_VISITED_HANDLER_H_
