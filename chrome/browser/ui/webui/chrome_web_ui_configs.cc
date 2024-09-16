// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_configs.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/about/about_ui.h"
#include "chrome/browser/ui/webui/accessibility/accessibility_ui.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/autofill_internals_ui.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/password_manager_internals_ui.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_ui.h"
#include "chrome/browser/ui/webui/components/components_ui.h"
#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals_ui.h"
#include "chrome/browser/ui/webui/download_internals/download_internals_ui.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/browser/ui/webui/local_state/local_state_ui.h"
#include "chrome/browser/ui/webui/location_internals/location_internals_ui.h"
#include "chrome/browser/ui/webui/memory_internals_ui.h"
#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_ui.h"
#include "chrome/browser/ui/webui/net_export_ui.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "chrome/browser/ui/webui/ntp_tiles_internals_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/predictors/predictors_ui.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_ui.h"
#include "chrome/browser/ui/webui/signin_internals_ui.h"
#include "chrome/browser/ui/webui/sync_internals/sync_internals_ui.h"
#include "chrome/browser/ui/webui/translate_internals/translate_internals_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/browser/ui/webui/user_actions/user_actions_ui.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "content/public/browser/webui_config_map.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/webui/media_router/cast_feedback_ui.h"
#endif
#include "chrome/browser/ui/lens/search_bubble_ui.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/browser/ui/webui/commerce/product_specifications_ui.h"
#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/feedback/feedback_ui.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_ui.h"
#include "chrome/browser/ui/webui/profile_internals/profile_internals_ui.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/browser/ui/webui/webui_gallery/webui_gallery_ui.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"  // nogncheck
#include "chrome/browser/ui/webui/crashes_ui.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/config/chrome_web_ui_configs_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_ui.h"
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/webui/signin/batch_upload_ui.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

void RegisterChromeWebUIConfigs() {
  // Don't add calls to `AddWebUIConfig()` for Ash-specific WebUIs here. Add
  // them in chrome_web_ui_configs_chromeos.cc.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::RegisterAshChromeWebUIConfigs();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto& map = content::WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(std::make_unique<AccessibilityUIConfig>());
  map.AddWebUIConfig(std::make_unique<AutofillInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<BrowsingTopicsInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ChromeURLsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ComponentsUIConfig>());
  map.AddWebUIConfig(std::make_unique<CreditsUIConfig>());
  map.AddWebUIConfig(std::make_unique<DataSharingInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<DownloadInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<FlagsUIConfig>());
  map.AddWebUIConfig(std::make_unique<LocalStateUIConfig>());
  map.AddWebUIConfig(std::make_unique<LocationInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<MemoryInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<MetricsInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<NTPTilesInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<OmniboxUIConfig>());
  map.AddWebUIConfig(std::make_unique<PasswordManagerInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<PredictorsUIConfig>());
  map.AddWebUIConfig(std::make_unique<NetExportUIConfig>());
  map.AddWebUIConfig(std::make_unique<NetInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<SegmentationInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<SignInInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<SyncInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<TranslateInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<UsbInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<UserActionsUIConfig>());
  map.AddWebUIConfig(std::make_unique<VersionUIConfig>());

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  map.AddWebUIConfig(std::make_unique<BluetoothInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<CrashesUIConfig>());
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  map.AddWebUIConfig(std::make_unique<media_router::CastFeedbackUIConfig>());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  map.AddWebUIConfig(std::make_unique<BookmarksSidePanelUIConfig>());
  map.AddWebUIConfig(std::make_unique<BookmarksUIConfig>());
  map.AddWebUIConfig(std::make_unique<CustomizeChromeUIConfig>());
  map.AddWebUIConfig(std::make_unique<DownloadsUIConfig>());
  map.AddWebUIConfig(std::make_unique<FeedbackUIConfig>());
  map.AddWebUIConfig(std::make_unique<HistoryUIConfig>());
  map.AddWebUIConfig(std::make_unique<HistoryClustersSidePanelUIConfig>());
  map.AddWebUIConfig(std::make_unique<lens::SearchBubbleUIConfig>());
  map.AddWebUIConfig(std::make_unique<OnDeviceInternalsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<commerce::ProductSpecificationsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ProfileInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ReadingListUIConfig>());
  map.AddWebUIConfig(std::make_unique<ShoppingInsightsSidePanelUIConfig>());
  map.AddWebUIConfig(std::make_unique<TabSearchUIConfig>());
  map.AddWebUIConfig(std::make_unique<TermsUIConfig>());
  map.AddWebUIConfig(std::make_unique<WebuiGalleryUIConfig>());
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OPENBSD)
  map.AddWebUIConfig(std::make_unique<LinuxProxyConfigUI>());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_OPENBSD)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  map.AddWebUIConfig(std::make_unique<extensions::ExtensionsUIConfig>());
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  map.AddWebUIConfig(std::make_unique<printing::PrintPreviewUIConfig>());
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  map.AddWebUIConfig(std::make_unique<CertificateManagerUIConfig>());
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  map.AddWebUIConfig(std::make_unique<WhatsNewUIConfig>());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  map.AddWebUIConfig(std::make_unique<BatchUploadUIConfig>());
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}
