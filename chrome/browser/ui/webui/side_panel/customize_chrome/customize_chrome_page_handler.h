// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/manta/manta_service.h"
#include "chrome/browser/manta/manta_status.h"
#include "chrome/browser/manta/proto/manta.pb.h"
#include "chrome/browser/manta/snapper_provider.h"
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

/**
 * Places where the chrome web store can be opened from in Customize Chrome.
 * This enum must match the numbering for NTPChromeWebStoreOpen in enums.xml.
 * These values are persisted to logs. Entries should not be renumbered, removed
 * or reused.
 */
enum class NtpChromeWebStoreOpen {
  kAppearance = 0,
  kCollections = 1,
  kMaxValue = kCollections,
};

class CustomizeChromePageHandler
    : public side_panel::mojom::CustomizeChromePageHandler,
      public NtpBackgroundServiceObserver,
      public ui::NativeThemeObserver,
      public ThemeServiceObserver,
      public NtpCustomBackgroundServiceObserver,
      public ui::SelectFileDialog::Listener {
 public:
  CustomizeChromePageHandler(
      mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
          pending_page_handler,
      mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
      NtpCustomBackgroundService* ntp_custom_background_service,
      content::WebContents* web_contents,
      const std::vector<std::pair<const std::string, int>> module_id_names);

  CustomizeChromePageHandler(const CustomizeChromePageHandler&) = delete;
  CustomizeChromePageHandler& operator=(const CustomizeChromePageHandler&) =
      delete;

  ~CustomizeChromePageHandler() override;

  void ScrollToSection(CustomizeChromeSection section);

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
  void GetBackgroundImages(const std::string& collection_id,
                           GetBackgroundImagesCallback callback) override;
  void ChooseLocalCustomBackground(
      ChooseLocalCustomBackgroundCallback callback) override;
  void RemoveBackgroundImage() override;
  void UpdateTheme() override;
  void OpenChromeWebStore() override;
  void OpenThirdPartyThemePage(const std::string& theme_id) override;
  void SetMostVisitedSettings(bool custom_links_enabled, bool visible) override;
  void UpdateMostVisitedSettings() override;
  void SetModulesVisible(bool visible) override;
  void SetModuleDisabled(const std::string& module_id, bool disabled) override;
  void UpdateModulesSettings() override;
  void UpdateScrollToSection() override;
  void SearchWallpaper(const std::string& query,
                       SearchWallpaperCallback callback) override;

 private:
  void LogEvent(NTPLoggingEventType event);

  void WallpaperSearchCallback(SearchWallpaperCallback callback,
                               std::unique_ptr<manta::proto::Response> response,
                               manta::MantaStatus manta_status);

  bool IsCustomLinksEnabled() const;
  bool IsShortcutsVisible() const;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;
  void OnNtpCustomBackgroundServiceShuttingDown() override;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  // SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  ChooseLocalCustomBackgroundCallback choose_local_custom_background_callback_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  raw_ptr<Profile> profile_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<NtpBackgroundService> ntp_background_service_;
  raw_ptr<manta::MantaService> manta_service_;
  std::unique_ptr<manta::SnapperProvider> snapper_provider_;
  GetBackgroundCollectionsCallback background_collections_callback_;
  base::TimeTicks background_collections_request_start_time_;
  std::string images_request_collection_id_;
  GetBackgroundImagesCallback background_images_callback_;
  base::TimeTicks background_images_request_start_time_;
  raw_ptr<ThemeService> theme_service_;
  const std::vector<std::pair<const std::string, int>> module_id_names_;
  // Caches a request to scroll to a section in case the front-end queries the
  // last requested section, e.g. during load.
  CustomizeChromeSection last_requested_section_ =
      CustomizeChromeSection::kUnspecified;

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

  base::WeakPtrFactory<CustomizeChromePageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
