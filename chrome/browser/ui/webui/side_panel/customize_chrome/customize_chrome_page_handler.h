// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/template_url_service_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;
class TemplateURLService;

/**
 * Places where the chrome web store can be opened from in Customize Chrome.
 * This enum must match the numbering for NTPChromeWebStoreOpen in enums.xml.
 * These values are persisted to logs. Entries should not be renumbered, removed
 * or reused.
 */
enum class NtpChromeWebStoreOpen {
  kAppearance = 0,
  kCollections = 1,
  kWritingEssentialsCollectionPage = 2,
  kWorkflowPlanningCategoryPage = 3,
  kShoppingCategoryPage = 4,
  kHomePage = 5,
  kMaxValue = kHomePage,
};

class CustomizeChromePageHandler
    : public side_panel::mojom::CustomizeChromePageHandler,
      public NtpBackgroundServiceObserver,
      public ui::NativeThemeObserver,
      public ThemeServiceObserver,
      public NtpCustomBackgroundServiceObserver,
      public TemplateURLServiceObserver,
      public ui::SelectFileDialog::Listener {
 public:
  // Returns whether the page handler can be constructed. Used to decide whether
  // the sidepanel should be allowed to show.
  static bool IsSupported(
      NtpCustomBackgroundService* ntp_custom_background_service,
      Profile* profile);

  CustomizeChromePageHandler(
      mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
          pending_page_handler,
      mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
      NtpCustomBackgroundService* ntp_custom_background_service,
      content::WebContents* web_contents,
      const std::vector<std::pair<const std::string, int>> module_id_names,
      std::optional<base::RepeatingCallback<void(const GURL&)>>
          open_url_callback = std::nullopt);

  CustomizeChromePageHandler(const CustomizeChromePageHandler&) = delete;
  CustomizeChromePageHandler& operator=(const CustomizeChromePageHandler&) =
      delete;

  ~CustomizeChromePageHandler() override;

  // Passes ScrollToSection calls to the CustomizeChromePage.
  void ScrollToSection(CustomizeChromeSection section);

  // Passes AttachedTabStateUpdated calls to the CustomizeChromePage.
  void AttachedTabStateUpdated(bool is_source_tab_first_party_ntp);

  // Helper method to determine if the search engine is overriding the first
  // party NTP.
  bool IsNtpManagedByThirdPartySearchEngine() const;

  // side_panel::mojom::CustomizeChromePageHandler:
  void SetDefaultColor() override;
  void SetFollowDeviceTheme(bool follow) override;
  void SetBackgroundImage(const std::string& attribution_1,
                          const std::string& attribution_2,
                          const GURL& attribution_url,
                          const GURL& image_url,
                          const GURL& thumbnail_url,
                          const std::string& collection_id) override;
  void SetDailyRefreshCollectionId(const std::string& collection_id) override;
  void GetBackgroundCollections(
      GetBackgroundCollectionsCallback callback) override;
  void GetReplacementCollectionPreviewImage(
      const std::string& collection_id,
      GetReplacementCollectionPreviewImageCallback callback) override;
  void GetBackgroundImages(const std::string& collection_id,
                           GetBackgroundImagesCallback callback) override;
  void ChooseLocalCustomBackground(
      ChooseLocalCustomBackgroundCallback callback) override;
  void RemoveBackgroundImage() override;
  void UpdateTheme() override;
  void OpenChromeWebStore() override;
  void OpenThirdPartyThemePage(const std::string& theme_id) override;
  void OpenChromeWebStoreCategoryPage(
      side_panel::mojom::ChromeWebStoreCategory category) override;
  void OpenChromeWebStoreCollectionPage(
      side_panel::mojom::ChromeWebStoreCollection collection) override;
  void OpenChromeWebStoreHomePage() override;
  void OpenNtpManagedByPage() override;
  void SetMostVisitedSettings(bool custom_links_enabled, bool visible) override;
  void UpdateMostVisitedSettings() override;
  void SetModulesVisible(bool visible) override;
  void SetModuleDisabled(const std::string& module_id, bool disabled) override;
  void UpdateModulesSettings() override;
  void UpdateScrollToSection() override;
  void UpdateAttachedTabState() override;
  void UpdateNtpManagedByName() override;

 private:
  void LogEvent(NTPLoggingEventType event);

  bool IsCustomLinksEnabled() const;
  bool IsShortcutsVisible() const;

  std::u16string GetManagingThirdPartyName() const;

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

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  ChooseLocalCustomBackgroundCallback choose_local_custom_background_callback_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  raw_ptr<Profile> profile_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<NtpBackgroundService> ntp_background_service_;
  GetBackgroundCollectionsCallback background_collections_callback_;
  base::TimeTicks background_collections_request_start_time_;
  std::string images_request_collection_id_;
  GetBackgroundImagesCallback background_images_callback_;
  base::TimeTicks background_images_request_start_time_;
  raw_ptr<TemplateURLService> template_url_service_;
  raw_ptr<ThemeService> theme_service_;
  const std::vector<std::pair<const std::string, int>> module_id_names_;

  // Caches a request to scroll to a section in case the front-end queries the
  // last requested section, e.g. during load.
  CustomizeChromeSection last_requested_section_ =
      CustomizeChromeSection::kUnspecified;

  // Caches the attached tab state provided to the handler, in cases where the
  // value needs to be requeried by the page.
  bool last_is_source_tab_first_party_ntp_ = true;

  PrefChangeRegistrar pref_change_registrar_;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observation_{this};

  mojo::Remote<side_panel::mojom::CustomizeChromePage> page_;
  mojo::Receiver<side_panel::mojom::CustomizeChromePageHandler> receiver_;

  // Callback used to open a URL.
  base::RepeatingCallback<void(const GURL&)> open_url_callback_;

  base::WeakPtrFactory<CustomizeChromePageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
