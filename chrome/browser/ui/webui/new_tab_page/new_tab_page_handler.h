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
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_observer.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/new_tab_page/promos/promo_service_observer.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller_observer.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/optimization_guide/core/model_execution/settings_enabled_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_provider_logos/logo_common.h"
#include "components/segmentation_platform/public/result.h"
#include "components/themes/ntp_background_service_observer.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class GURL;
class OptimizationGuideKeyedService;
class Profile;
class MicrosoftAuthService;
class NTPUserDataLogger;
class NewTabPageFeaturePromoHelper;

namespace content {
class WebContents;
}  // namespace content

namespace new_tab_footer {
class NewTabFooterController;
}

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

class NewTabPageHandler
    : public new_tab_page::mojom::PageHandler,
      public ui::NativeThemeObserver,
      public ThemeServiceObserver,
      public NtpCustomBackgroundServiceObserver,
      public PromoServiceObserver,
      public optimization_guide::SettingsEnabledObserver,
      public MicrosoftAuthServiceObserver,
      public new_tab_footer::NewTabFooterControllerObserver {
 public:
  NewTabPageHandler(mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
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
                    const base::Time& ntp_navigation_start_time,
                    const std::vector<ntp::ModuleIdDetail>* module_id_details);

  NewTabPageHandler(const NewTabPageHandler&) = delete;
  NewTabPageHandler& operator=(const NewTabPageHandler&) = delete;

  ~NewTabPageHandler() override;

  // Histograms being recorded when a module is dismissed or restored.
  static const char kModuleDismissedHistogram[];
  static const char kModuleRestoredHistogram[];

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // This method should be called before the tab is deleted.
  void TabWillDelete();

  // Called when a child page wants to bind its interface in `page_`, so they
  // can communicate via Mojo.
  void ConnectToParentDocument(
      mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
          child_untrusted_document_remote);

  // new_tab_page::mojom::PageHandler:
  void SetMostVisitedSettings(ntp_tiles::TileType type, bool visible) override;
  void GetMostVisitedSettings(GetMostVisitedSettingsCallback callback) override;
  void GetDoodle(GetDoodleCallback callback) override;
  void UpdatePromoData() override;
  void BlocklistPromo(const std::string& promo_id) override;
  void UndoBlocklistPromo(const std::string& promo_id) override;
  void OnDismissModule(const std::string& module_id) override;
  void OnRestoreModule(const std::string& module_id) override;
  void SetModulesVisible(bool visible) override;
  void SetModuleDisabled(const std::string& module_id, bool disabled) override;
  void UpdateDisabledModules() override;
  void UpdateFooterVisibility() override;
  void OnModulesLoadedWithData(
      const std::vector<std::string>& module_ids) override;
  void OnModuleUsed(const std::string& module_id) override;
  void GetModulesIdNames(GetModulesIdNamesCallback callback) override;
  void SetModulesOrder(const std::vector<std::string>& module_ids) override;
  void GetModulesOrder(GetModulesOrderCallback callback) override;
  void UpdateModulesLoadable() override;
  void UpdateActionChipsVisibility() override;
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
  void IncrementComposeButtonShownCount() override;
  void MaybeTriggerAutomaticCustomizeChromePromo() override;

 private:
  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

  // PromoServiceObserver:
  void OnPromoDataUpdated() override;
  void OnPromoServiceShuttingDown() override;

  // SettingsEnabledObserver:
  void OnChangeInFeatureCurrentlyEnabledState(bool is_now_enabled) override;

  // MicrosoftAuthServiceObserver:
  void OnAuthStateUpdated() override;

  // new_tab_footer::NewTabFooterControllerObserver:
  void OnFooterVisibilityUpdated(bool visible) override;

  void OnLogoAvailable(
      GetDoodleCallback callback,
      search_provider_logos::LogoCallbackReason type,
      const std::optional<search_provider_logos::EncodedLogo>& logo);

  // Called when the embedding BrowserWindowInterface has changed.
  void OnBrowserWindowInterfaceChanged();

  void LogEvent(NTPLoggingEventType event);

  typedef base::OnceCallback<void(bool success,
                                  std::optional<std::string> body)>
      OnFetchResultCallback;
  void Fetch(const GURL& url, OnFetchResultCallback on_result);
  void OnFetchResult(const network::SimpleURLLoader* loader,
                     OnFetchResultCallback on_result,
                     std::optional<std::string> body);
  void OnLogFetchResult(OnDoodleImageRenderedCallback callback,
                        bool success,
                        std::optional<std::string> body);

  ntp_tiles::TileType GetTileType() const;
  bool IsActionChipsVisible() const;
  bool IsShortcutsVisible() const;
  void MaybeLaunchInteractionSurvey(std::string_view interaction,
                                    const std::string& module_id,
                                    int delay_time_ms = 0);
  void MaybeShowWebstoreToast();
  void RecordModuleInteraction(const std::string& module_id);
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

  void SetModuleHidden(const std::string& module_id, bool hidden);

  // Returns a list of module ids that are eligible for removal, which is
  // determined the module staleness and the staleness threshold.
  std::vector<std::string> GetModulesEligibleForRemoval() const;
  void SetStaleModulesDisabled(const std::vector<std::string>& module_ids,
                               bool disabled);

  // Synchronizes Microsoft module enablement with their current authentication
  // state. The return value indicates whether the modules should be considered
  // loadable.
  bool SyncMicrosoftModulesWithAuth();

  raw_ptr<NtpCustomBackgroundService> const ntp_custom_background_service_;
  raw_ptr<search_provider_logos::LogoService> const logo_service_;
  raw_ptr<const ui::ThemeProvider> const theme_provider_;
  raw_ptr<ThemeService> const theme_service_;
  raw_ptr<syncer::SyncService> const sync_service_;
  raw_ptr<segmentation_platform::SegmentationPlatformService> const
      segmentation_platform_service_;
  GURL last_blocklisted_;
  std::optional<base::TimeTicks> one_google_bar_load_start_time_;
  raw_ptr<Profile> const profile_;
  raw_ptr<content::WebContents> const web_contents_;
  std::unique_ptr<NewTabPageFeaturePromoHelper> feature_promo_helper_;
  base::Time ntp_navigation_start_time_;
  raw_ptr<const std::vector<ntp::ModuleIdDetail>> const module_id_details_;
  NTPUserDataLogger logger_;
  std::unordered_map<const network::SimpleURLLoader*,
                     std::unique_ptr<network::SimpleURLLoader>>
      loader_map_;
  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;
  raw_ptr<PromoService> promo_service_;
  raw_ptr<MicrosoftAuthService> const microsoft_auth_service_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observation_{this};
  base::ScopedObservation<PromoService, PromoServiceObserver>
      promo_service_observation_{this};
  base::ScopedObservation<MicrosoftAuthService, MicrosoftAuthServiceObserver>
      microsoft_auth_service_observation_{this};
  base::ScopedObservation<new_tab_footer::NewTabFooterController,
                          new_tab_footer::NewTabFooterControllerObserver>
      footer_controller_observation_{this};
  std::optional<base::TimeTicks> promo_load_start_time_;
  base::Value::Dict interaction_module_id_trigger_dict_;
  // Notifies this when the browser window context changes.
  base::CallbackListSubscription browser_window_changed_subscription_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<new_tab_page::mojom::Page> page_;
  mojo::Receiver<new_tab_page::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<NewTabPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_
