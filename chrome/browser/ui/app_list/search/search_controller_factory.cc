// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller_factory.h"

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"
#include "chrome/browser/ui/app_list/search/app_search_provider.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_data_search_provider.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_search_provider.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcuts_search_provider.h"
#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_provider.h"
#include "chrome/browser/ui/app_list/search/assistant_search_provider.h"
#include "chrome/browser/ui/app_list/search/assistant_text_search_provider.h"
#include "chrome/browser/ui/app_list/search/drive_quick_access_provider.h"
#include "chrome/browser/ui/app_list/search/files/drive_zero_state_provider.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_provider.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/omnibox_provider.h"
#include "chrome/browser/ui/app_list/search/os_settings_provider.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/settings_shortcut/settings_shortcut_provider.h"
#include "chrome/browser/ui/app_list/search/zero_state_file_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/arc/arc_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace app_list {

namespace {

// Maximum number of results to show in each mixer group.

// A generic value for max results, which is large enough to not interfere with
// the actual results displayed. This should be used by providers that configure
// their maximum number of results within the provider itself.
//
// TODO(crbug.com/1028447): Use this value for other providers that don't really
// need a max results limit. Eventually, make this an optional constraint on a
// Group.
constexpr size_t kGenericMaxResults = 10;

// Some app results may be blocklisted (e.g. continue reading) for rendering
// in some UI, so we need to allow returning more results than actual maximum
// number of results to be displayed in UI.
constexpr size_t kMaxAppsGroupResults = 7;
constexpr size_t kMaxLauncherSearchResults = 4;
// We need twice as many ZeroState and Drive file results as we need
// duplicates of these results for the suggestion chips.
constexpr size_t kMaxZeroStateFileResults = 20;
constexpr size_t kMaxDriveZeroStateResults = 10;
constexpr size_t kMaxAppReinstallSearchResults = 1;
// We show up to 6 Play Store results. However, part of Play Store results may
// be filtered out because they may correspond to already installed Web apps. So
// we request twice as many Play Store apps as we can show. Note that this still
// doesn't guarantee that all 6 positions will be filled, as we might in theory
// filter out more than half of results.
// TODO(753947): Consider progressive algorithm of getting Play Store results.
constexpr size_t kMaxPlayStoreResults = 12;

// TODO(warx): Need UX spec.
constexpr size_t kMaxAppDataResults = 4;
constexpr size_t kMaxAppShortcutResults = 4;

// Assistant provides a single search result when launcher chip integration is
// enabled from its internal cache of conversation starters.
constexpr size_t kMaxAssistantChipResults = 1;

constexpr size_t kMaxAssistantTextResults = 1;

// TODO(wutao): Need UX spec.
constexpr size_t kMaxSettingsShortcutResults = 6;

}  // namespace

std::unique_ptr<SearchController> CreateSearchController(
    Profile* profile,
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller,
    ash::AppListNotifier* notifier) {
  std::unique_ptr<SearchController> controller =
      std::make_unique<SearchController>(model_updater, list_controller,
                                         notifier, profile);

  // Set up rankers for search results.
  controller->InitializeRankers();

  size_t apps_group_id = controller->AddGroup(kMaxAppsGroupResults);
  size_t answer_card_group_id = controller->AddGroup(1);

  size_t omnibox_group_id = controller->AddGroup(
      ash::AppListConfig::instance().max_search_result_list_items());

  // Add search providers.
  controller->AddProvider(
      apps_group_id, std::make_unique<AppSearchProvider>(
                         profile, list_controller,
                         base::DefaultClock::GetInstance(), model_updater));
  if (app_list_features::IsAnswerCardEnabled()) {
    controller->AddProvider(answer_card_group_id,
                            std::make_unique<AnswerCardSearchProvider>(
                                profile, model_updater, list_controller));
  }

  controller->AddProvider(omnibox_group_id, std::make_unique<OmniboxProvider>(
                                                profile, list_controller));

  // The Assistant search provider currently only contributes search results
  // when launcher chip integration is enabled.
  if (chromeos::assistant::features::IsLauncherChipIntegrationEnabled()) {
    size_t assistant_group_id = controller->AddGroup(kMaxAssistantChipResults);
    controller->AddProvider(assistant_group_id,
                            std::make_unique<AssistantSearchProvider>());
  }

  if (app_list_features::IsAssistantSearchEnabled()) {
    size_t assistant_group_id = controller->AddGroup(kMaxAssistantTextResults);
    controller->AddProvider(assistant_group_id,
                            std::make_unique<AssistantTextSearchProvider>());
  }

  // LauncherSearchProvider is added only when not in guest
  // session and running on Chrome OS.
  if (!profile->IsGuestSession()) {
    size_t search_api_group_id =
        controller->AddGroup(kMaxLauncherSearchResults);
    controller->AddProvider(search_api_group_id,
                            std::make_unique<LauncherSearchProvider>(profile));
  }

  // reinstallation candidates for Arc++ apps.
  if (app_list_features::IsAppReinstallZeroStateEnabled() &&
      arc::IsArcAllowedForProfile(profile)) {
    size_t recommended_app_group_id =
        controller->AddGroup(kMaxAppReinstallSearchResults);
    controller->AddProvider(recommended_app_group_id,
                            std::make_unique<ArcAppReinstallSearchProvider>(
                                profile, kMaxAppReinstallSearchResults));
  }

  // Set same boost as apps group since Play store results are placed
  // with apps.
  size_t playstore_api_group_id = controller->AddGroup(kMaxPlayStoreResults);
  controller->AddProvider(playstore_api_group_id,
                          std::make_unique<ArcPlayStoreSearchProvider>(
                              kMaxPlayStoreResults, profile, list_controller));

  if (app_list_features::IsAppDataSearchEnabled()) {
    size_t app_data_api_group_id = controller->AddGroup(kMaxAppDataResults);
    controller->AddProvider(app_data_api_group_id,
                            std::make_unique<ArcAppDataSearchProvider>(
                                kMaxAppDataResults, list_controller));
  }

  // TODO(crbug.com/1028447): Remove the settings shortcut provider, superseded
  // by OsSettingsProvider.
  if (app_list_features::IsSettingsShortcutSearchEnabled()) {
    size_t settings_shortcut_group_id =
        controller->AddGroup(kMaxSettingsShortcutResults);
    controller->AddProvider(
        settings_shortcut_group_id,
        std::make_unique<SettingsShortcutProvider>(profile));
  }

  if (arc::IsArcAllowedForProfile(profile)) {
    size_t app_shortcut_group_id = controller->AddGroup(kMaxAppShortcutResults);
    controller->AddProvider(
        app_shortcut_group_id,
        std::make_unique<ArcAppShortcutsSearchProvider>(
            kMaxAppShortcutResults, profile, list_controller));
  }

  // This flag controls whether files are shown alongside Omnibox recent queries
  // in the launcher. If enabled, Omnibox recent queries have their relevance
  // scores changed to fit with these providers.
  if (app_list_features::IsZeroStateMixedTypesRankerEnabled()) {
    size_t zero_state_files_group_id =
        controller->AddGroup(kMaxZeroStateFileResults);
    controller->AddProvider(zero_state_files_group_id,
                            std::make_unique<ZeroStateFileProvider>(profile));
    size_t drive_zero_state_group_id =
        controller->AddGroup(kMaxDriveZeroStateResults);
    controller->AddProvider(
        drive_zero_state_group_id,
        std::make_unique<DriveZeroStateProvider>(
            profile, controller.get(),
            content::BrowserContext::GetDefaultStoragePartition(profile)
                ->GetURLLoaderFactoryForBrowserProcess()));
  }

  if (app_list_features::IsLauncherSettingsSearchEnabled()) {
    size_t os_settings_search_group_id =
        controller->AddGroup(kGenericMaxResults);
    controller->AddProvider(os_settings_search_group_id,
                            std::make_unique<OsSettingsProvider>(profile));
  }

  return controller;
}

}  // namespace app_list
