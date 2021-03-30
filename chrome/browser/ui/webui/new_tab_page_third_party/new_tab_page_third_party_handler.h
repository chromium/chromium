// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HANDLER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party.mojom.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/section_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

class NewTabPageThirdPartyHandler
    : public new_tab_page_third_party::mojom::PageHandler,
      public ntp_tiles::MostVisitedSites::Observer,
      public ThemeServiceObserver,
      public ui::NativeThemeObserver {
 public:
  NewTabPageThirdPartyHandler(
      mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<new_tab_page_third_party::mojom::Page> pending_page,
      Profile* profile,
      content::WebContents* web_contents,
      const base::Time& ntp_navigation_start_time);
  ~NewTabPageThirdPartyHandler() override;

  // new_tab_page_third_party::mojom::PageHandler:
  void DeleteMostVisitedTile(const GURL& url) override;
  void OnMostVisitedTilesRendered(
      std::vector<new_tab_page_third_party::mojom::MostVisitedTilePtr> tiles,
      double time) override;
  void OnMostVisitedTileNavigation(
      new_tab_page_third_party::mojom::MostVisitedTilePtr tile,
      uint32_t index,
      uint8_t mouse_button,
      bool alt_key,
      bool ctrl_key,
      bool meta_key,
      bool shift_key) override;
  void RestoreMostVisitedDefaults() override;
  void UndoMostVisitedTileAction() override;
  void UpdateMostVisitedTiles() override;
  void UpdateTheme() override;

 private:
  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // ntp_tiles::MostVisitedSites::Observer implementation.
  void OnURLsAvailable(
      const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
          sections) override;
  void OnIconMadeAvailable(const GURL& site_url) override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  void NotifyAboutMostVisitedTiles();
  void NotifyAboutTheme();

  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites_;
  Profile* profile_;
  content::WebContents* web_contents_;
  ntp_tiles::NTPTilesVector most_visited_tiles_;
  NTPUserDataLogger logger_;
  base::Time ntp_navigation_start_time_;
  GURL last_blocklisted_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<new_tab_page_third_party::mojom::Page> page_;
  mojo::Receiver<new_tab_page_third_party::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<NewTabPageThirdPartyHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NewTabPageThirdPartyHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HANDLER_H_
