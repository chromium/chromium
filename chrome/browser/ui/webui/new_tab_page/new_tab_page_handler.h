// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_

#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_observer.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/promos/promo_service_observer.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/search_provider_logos/logo_common.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class GURL;
class InstantService;
class NtpBackgroundService;
class Profile;
class NTPUserDataLogger;

namespace content {
class WebContents;
}  // namespace content

namespace search_provider_logos {
class LogoService;
}  // namespace search_provider_logos

class NewTabPageHandler : public new_tab_page::mojom::PageHandler,
                          public InstantServiceObserver,
                          public NtpBackgroundServiceObserver,
                          public OneGoogleBarServiceObserver,
                          public ui::SelectFileDialog::Listener,
                          public PromoServiceObserver {
 public:
  NewTabPageHandler(mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
                        pending_page_handler,
                    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
                    Profile* profile,
                    InstantService* instant_service,
                    content::WebContents* web_contents,
                    const base::Time& ntp_navigation_start_time);
  ~NewTabPageHandler() override;

  // Histograms being recorded when a module is dismissed or restored.
  static const char kModuleDismissedHistogram[];
  static const char kModuleRestoredHistogram[];

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // new_tab_page::mojom::PageHandler:
  void AddMostVisitedTile(const GURL& url,
                          const std::string& title,
                          AddMostVisitedTileCallback callback) override;
  void DeleteMostVisitedTile(const GURL& url) override;
  void RestoreMostVisitedDefaults() override;
  void ReorderMostVisitedTile(const GURL& url, uint8_t new_pos) override;
  void SetMostVisitedSettings(bool custom_links_enabled, bool visible) override;
  void UndoMostVisitedTileAction() override;
  void UpdateMostVisitedInfo() override;
  void UpdateMostVisitedTile(const GURL& url,
                             const GURL& new_url,
                             const std::string& new_title,
                             UpdateMostVisitedTileCallback callback) override;
  void SetBackgroundImage(const std::string& attribution_1,
                          const std::string& attribution_2,
                          const GURL& attribution_url,
                          const GURL& image_url) override;
  void SetDailyRefreshCollectionId(const std::string& collection_id) override;
  void SetNoBackgroundImage() override;
  void GetBackgroundCollections(
      GetBackgroundCollectionsCallback callback) override;
  void GetBackgroundImages(const std::string& collection_id,
                           GetBackgroundImagesCallback callback) override;
  void GetDoodle(GetDoodleCallback callback) override;
  void ChooseLocalCustomBackground(
      ChooseLocalCustomBackgroundCallback callback) override;
  void GetOneGoogleBarParts(const std::string& ogdeb_value,
                            GetOneGoogleBarPartsCallback callback) override;
  void GetPromo(GetPromoCallback callback) override;
  void OnDismissModule(const std::string& module_id) override;
  void OnRestoreModule(const std::string& module_id) override;
  void SetModulesVisible(bool visible) override;
  void SetModuleDisabled(const std::string& module_id, bool disabled) override;
  void UpdateDisabledModules() override;
  void OnModulesLoadedWithData() override;
  void OnAppRendered(double time) override;
  void OnMostVisitedTilesRendered(
      std::vector<new_tab_page::mojom::MostVisitedTilePtr> tiles,
      double time) override;
  void OnOneGoogleBarRendered(double time) override;
  void OnPromoRendered(double time,
                       const base::Optional<GURL>& log_url) override;
  void OnMostVisitedTileNavigation(new_tab_page::mojom::MostVisitedTilePtr tile,
                                   uint32_t index,
                                   uint8_t mouse_button,
                                   bool alt_key,
                                   bool ctrl_key,
                                   bool meta_key,
                                   bool shift_key) override;
  void OnCustomizeDialogAction(
      new_tab_page::mojom::CustomizeDialogAction action) override;
  void OnDoodleImageClicked(new_tab_page::mojom::DoodleImageType type,
                            const base::Optional<GURL>& log_url) override;
  void OnDoodleImageRendered(new_tab_page::mojom::DoodleImageType type,
                             double time,
                             const GURL& log_url,
                             OnDoodleImageRenderedCallback callback) override;
  void OnDoodleShared(new_tab_page::mojom::DoodleShareChannel channel,
                      const std::string& doodle_id,
                      const base::Optional<std::string>& share_id) override;
  void OnPromoLinkClicked() override;
  void OnVoiceSearchAction(
      new_tab_page::mojom::VoiceSearchAction action) override;
  void OnVoiceSearchError(new_tab_page::mojom::VoiceSearchError error) override;

 private:
  // InstantServiceObserver:
  void NtpThemeChanged(const NtpTheme& theme) override;
  void MostVisitedInfoChanged(const InstantMostVisitedInfo& info) override;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  // OneGoogleBarServiceObserver:
  void OnOneGoogleBarDataUpdated() override;
  void OnOneGoogleBarServiceShuttingDown() override;

  // PromoServiceObserver:
  void OnPromoDataUpdated() override;
  void OnPromoServiceShuttingDown() override;

  // SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  void OnLogoAvailable(
      GetDoodleCallback callback,
      search_provider_logos::LogoCallbackReason type,
      const base::Optional<search_provider_logos::EncodedLogo>& logo);

  void LogEvent(NTPLoggingEventType event);

  typedef base::OnceCallback<void(bool success,
                                  std::unique_ptr<std::string> body)>
      OnFetchResultCallback;
  void Fetch(const GURL& url, OnFetchResultCallback on_result);
  void OnFetchResult(const network::SimpleURLLoader* loader,
                     OnFetchResultCallback on_result,
                     std::unique_ptr<std::string> body);
  void OnLogFetchResult(OnDoodleImageRenderedCallback callback,
                        bool success,
                        std::unique_ptr<std::string> body);

  ChooseLocalCustomBackgroundCallback choose_local_custom_background_callback_;
  InstantService* instant_service_;
  NtpBackgroundService* ntp_background_service_;
  search_provider_logos::LogoService* logo_service_;
  GURL last_blocklisted_;
  GetBackgroundCollectionsCallback background_collections_callback_;
  base::TimeTicks background_collections_request_start_time_;
  std::string images_request_collection_id_;
  GetBackgroundImagesCallback background_images_callback_;
  base::TimeTicks background_images_request_start_time_;
  std::vector<GetOneGoogleBarPartsCallback> one_google_bar_parts_callbacks_;
  OneGoogleBarService* one_google_bar_service_;
  base::ScopedObservation<OneGoogleBarService, OneGoogleBarServiceObserver>
      one_google_bar_service_observation_{this};
  base::Optional<base::TimeTicks> one_google_bar_load_start_time_;
  Profile* profile_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  content::WebContents* web_contents_;
  base::Time ntp_navigation_start_time_;
  NTPUserDataLogger logger_;
  std::unordered_map<const network::SimpleURLLoader*,
                     std::unique_ptr<network::SimpleURLLoader>>
      loader_map_;
  std::vector<GetPromoCallback> promo_callbacks_;
  PromoService* promo_service_;
  base::ScopedObservation<PromoService, PromoServiceObserver>
      promo_service_observation_{this};
  base::Optional<base::TimeTicks> promo_load_start_time_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<new_tab_page::mojom::Page> page_;
  mojo::Receiver<new_tab_page::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<NewTabPageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NewTabPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_
