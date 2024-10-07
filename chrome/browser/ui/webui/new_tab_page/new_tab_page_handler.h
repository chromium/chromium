// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/new_tab_page/promos/promo_service_observer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/optimization_guide/core/model_execution/settings_enabled_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_provider_logos/logo_common.h"
#include "components/segmentation_platform/public/result.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class GURL;
class NtpBackgroundService;
class Profile;
class NTPUserDataLogger;
class NewTabPageFeaturePromoHelper;

namespace content {
class WebContents;
}  // namespace content

namespace customize_chrome {
class SidePanelController;
}  // namespace customize_chrome

namespace search_provider_logos {
class LogoService;
}  // namespace search_provider_logos

namespace segmentation_platform {
class SegmentationPlatformService;
}  // namespace segmentation_platform

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ui {
class ThemeProvider;
}  // namespace ui

class NewTabPageHandler : public new_tab_page::mojom::PageHandler,
                          public ui::NativeThemeObserver,
                          public ThemeServiceObserver,
                          public NtpCustomBackgroundServiceObserver,
                          public NtpBackgroundServiceObserver,
                          public ui::SelectFileDialog::Listener,
                          public PromoServiceObserver,
                          public optimization_guide::SettingsEnabledObserver {
 public:
  NewTabPageHandler(
      mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
      Profile* profile,
      NtpCustomBackgroundService* ntp_custom_background_service,
      ThemeService* theme_service,
      search_provider_logos::LogoService* logo_service,
      syncer::SyncService* sync_service,
      segmentation_platform::SegmentationPlatformService*
          segmentation_platform_service,
      content::WebContents* web_contents,
      std::unique_ptr<NewTabPageFeaturePromoHelper>
          customize_chrome_feature_promo_helper,
      const base::Time& ntp_navigation_start_time,
      const std::vector<std::pair<const std::string, int>>* module_id_names,
      customize_chrome::SidePanelController*
          customize_chrome_side_panel_controller);

  NewTabPageHandler(const NewTabPageHandler&) = delete;
  NewTabPageHandler& operator=(const NewTabPageHandler&) = delete;

  ~NewTabPageHandler() override;

  // Histograms being recorded when a module is dismissed or restored.
  static const char kModuleDismissedHistogram[];
  static const char kModuleRestoredHistogram[];

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // This method should be called before the tab is deleted.
  void TabWillDelete();

  // new_tab_page::mojom::PageHandler:
  void SetMostVisitedSettings(bool custom_links_enabled, bool visible) override;
  void GetMostVisitedSettings(GetMostVisitedSettingsCallback callback) override;
  void SetBackgroundImage(const std::string& attribution_1,
                          const std::string& attribution_2,
                          const GURL& attribution_url,
                          const GURL& image_url,
                          const GURL& thumbnail_ur,
                          const std::string& collection_id) override;
  void SetDailyRefreshCollectionId(const std::string& collection_id) override;
  void SetNoBackgroundImage() override;
  void RevertBackgroundChanges() override;
  void ConfirmBackgroundChanges() override;
  void GetBackgroundCollections(
      GetBackgroundCollectionsCallback callback) override;
  void GetBackgroundImages(const std::string& collection_id,
                           GetBackgroundImagesCallback callback) override;
  void GetDoodle(GetDoodleCallback callback) override;
  void ChooseLocalCustomBackground(
      ChooseLocalCustomBackgroundCallback callback) override;
  void UpdatePromoData() override;
  void BlocklistPromo(const std::string& promo_id) override;
  void UndoBlocklistPromo(const std::string& promo_id) override;
  void OnDismissModule(const std::string& module_id) override;
  void OnRestoreModule(const std::string& module_id) override;
  void SetModulesVisible(bool visible) override;
  void SetModuleDisabled(const std::string& module_id, bool disabled) override;
  void UpdateDisabledModules() override;
  void OnModulesLoadedWithData(
      const std::vector<std::string>& module_ids) override;
  void OnModuleUsed(const std::string& module_id) override;
  void GetModulesIdNames(GetModulesIdNamesCallback callback) override;
  void SetModulesOrder(const std::vector<std::string>& module_ids) override;
  void GetModulesOrder(GetModulesOrderCallback callback) override;
  void SetCustomizeChromeSidePanelVisible(
      bool visible,
      new_tab_page::mojom::CustomizeChromeSection section) override;
  void IncrementCustomizeChromeButtonOpenCount() override;
  void GetMobilePromoQrCode(GetMobilePromoQrCodeCallback callback) override;
  void MaybeShowFeaturePromo(
      new_tab_page::mojom::IphFeature iph_feature) override;
  void IncrementWallpaperSearchButtonShownCount() override;
  void OnAppRendered(double time) override;
  void OnOneGoogleBarRendered(double time) override;
  void OnPromoRendered(double time,
                       const std::optional<GURL>& log_url) override;
  void OnCustomizeDialogAction(
      new_tab_page::mojom::CustomizeDialogAction action) override;
  void OnDoodleImageClicked(new_tab_page::mojom::DoodleImageType type,
                            const std::optional<GURL>& log_url) override;
  void OnDoodleImageRendered(new_tab_page::mojom::DoodleImageType type,
                             double time,
                             const GURL& log_url,
                             OnDoodleImageRenderedCallback callback) override;
  void OnDoodleShared(new_tab_page::mojom::DoodleShareChannel channel,
                      const std::string& doodle_id,
                      const std::optional<std::string>& share_id) override;
  void OnPromoLinkClicked() override;

 private:
  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  // PromoServiceObserver:
  void OnPromoDataUpdated() override;
  void OnPromoServiceShuttingDown() override;

  // SettingsEnabledObserver:
  void OnChangeInFeatureCurrentlyEnabledState(bool is_now_enabled) override;

  // SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  void OnLogoAvailable(
      GetDoodleCallback callback,
      search_provider_logos::LogoCallbackReason type,
      const std::optional<search_provider_logos::EncodedLogo>& logo);

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

  bool IsCustomLinksEnabled() const;
  bool IsShortcutsVisible() const;
  void NotifyCustomizeChromeSidePanelVisibilityChanged(bool is_open);
  void MaybeLaunchInteractionSurvey(std::string_view interaction,
                                    const std::string& module_id,
                                    int delay_time_ms = 0);
  void MaybeShowWebstoreToast();
  void IncrementDictPrefKeyCount(const std::string& pref_name,
                                 const std::string& key);

  // Returns a HaTS trigger id associated with the given combination of user
  // interaction and module id if one exists, or nullptr otherwise to indicate
  // that there is no configured survey trigger id for such combination. The
  // valid interaction names are defined in `kModuleInteractionNames`. The valid
  // module id strings are listed in `ntp::MakeModuleIdNames`.
  const std::string& GetSurveyTriggerIdForModuleAndInteraction(
      std::string_view interaction,
      const std::string& module_id);

  // Check if user is eligible to see a mobile promo generated locally. The
  // callback is a mojo callback that must be called in all cases.
  void CheckIfUserEligibleForMobilePromo(GetMobilePromoQrCodeCallback callback);
  // Handle the response from the segmentation platform querying mobile promo
  // status. The callback is a mojo callback that must be called in all cases.
  void HandleMobilePromoSegmentationResponse(
      GetMobilePromoQrCodeCallback callback,
      const segmentation_platform::ClassificationResult& result);

  ChooseLocalCustomBackgroundCallback choose_local_custom_background_callback_;
  raw_ptr<NtpBackgroundService> ntp_background_service_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  raw_ptr<search_provider_logos::LogoService> logo_service_;
  raw_ptr<const ui::ThemeProvider> theme_provider_;
  raw_ptr<ThemeService> theme_service_;
  raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_;
  GURL last_blocklisted_;
  GetBackgroundCollectionsCallback background_collections_callback_;
  base::TimeTicks background_collections_request_start_time_;
  std::string images_request_collection_id_;
  GetBackgroundImagesCallback background_images_callback_;
  base::TimeTicks background_images_request_start_time_;
  std::optional<base::TimeTicks> one_google_bar_load_start_time_;
  raw_ptr<Profile> profile_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NewTabPageFeaturePromoHelper> feature_promo_helper_;
  base::Time ntp_navigation_start_time_;
  raw_ptr<const std::vector<std::pair<const std::string, int>>>
      module_id_names_;
  NTPUserDataLogger logger_;
  std::unordered_map<const network::SimpleURLLoader*,
                     std::unique_ptr<network::SimpleURLLoader>>
      loader_map_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PromoService> promo_service_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observation_{this};
  base::ScopedObservation<PromoService, PromoServiceObserver>
      promo_service_observation_{this};
  std::optional<base::TimeTicks> promo_load_start_time_;
  base::Value::Dict interaction_module_id_trigger_dict_;
  raw_ptr<customize_chrome::SidePanelController>
      customize_chrome_side_panel_controller_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<new_tab_page::mojom::Page> page_;
  mojo::Receiver<new_tab_page::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<NewTabPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_
