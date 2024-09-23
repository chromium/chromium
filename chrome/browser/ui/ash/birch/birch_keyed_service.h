// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_H_

#include <memory>

#include "ash/birch/birch_client.h"
#include "ash/shell_observer.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/models/image_model.h"

class Profile;

namespace ash {

class BirchCalendarProvider;
class BirchFileSuggestProvider;
class BirchLastActiveProvider;
class BirchLostMediaProvider;
class BirchMostVisitedProvider;
class BirchRecentTabsProvider;
class BirchReleaseNotesProvider;
class BirchSelfShareProvider;
class RefreshTokenWaiter;
class Shell;

// A keyed service which is used to manage data providers for the birch feature.
// Fetched data will be sent to the `BirchModel` to be stored.
class BirchKeyedService : public KeyedService,
                          public ShellObserver,
                          public BirchClient {
 public:
  explicit BirchKeyedService(Profile* profile);
  BirchKeyedService(const BirchKeyedService&) = delete;
  BirchKeyedService& operator=(const BirchKeyedService&) = delete;
  ~BirchKeyedService() override;

  BirchFileSuggestProvider* GetFileSuggestProviderForTest() {
    return file_suggest_provider_.get();
  }

  BirchReleaseNotesProvider* GetReleaseNotesProviderForTest() {
    return release_notes_provider_.get();
  }

  // ShellObserver:
  void OnShellDestroying() override;

  // BirchClient:
  BirchDataProvider* GetCalendarProvider() override;
  BirchDataProvider* GetFileSuggestProvider() override;
  BirchDataProvider* GetRecentTabsProvider() override;
  BirchDataProvider* GetLastActiveProvider() override;
  BirchDataProvider* GetMostVisitedProvider() override;
  BirchDataProvider* GetReleaseNotesProvider() override;
  BirchDataProvider* GetSelfShareProvider() override;
  BirchDataProvider* GetLostMediaProvider() override;

  void WaitForRefreshTokens(base::OnceClosure callback) override;
  base::FilePath GetRemovedItemsFilePath() override;
  void RemoveFileItemFromLauncher(const base::FilePath& path) override;

  void GetFaviconImage(
      const GURL& page_url,
      const bool is_page_url,
      base::OnceCallback<void(const ui::ImageModel&)> callback) override;

  ui::ImageModel GetChromeBackupIcon() override;

  void set_calendar_provider_for_test(BirchDataProvider* provider) {
    calendar_provider_for_test_ = provider;
  }
  void set_file_suggest_provider_for_test(BirchDataProvider* provider) {
    file_suggest_provider_for_test_ = provider;
  }
  void set_recent_tabs_provider_for_test(BirchDataProvider* provider) {
    recent_tabs_provider_for_test_ = provider;
  }
  void set_last_active_provider_for_test(BirchDataProvider* provider) {
    last_active_provider_for_test_ = provider;
  }
  void set_most_visited_provider_for_test(BirchDataProvider* provider) {
    most_visited_provider_for_test_ = provider;
  }
  void set_self_share_provider_for_test(BirchDataProvider* provider) {
    self_share_provider_for_test_ = provider;
  }
  void set_lost_media_provider_for_test(BirchDataProvider* provider) {
    lost_media_provider_for_test_ = provider;
  }
  void set_release_notes_provider_for_test(BirchDataProvider* provider) {
    release_notes_provider_for_test_ = provider;
  }

 private:
  void ShutdownBirch();

  // Whether shutdown of BirchKeyedService has already begun.
  bool is_shutdown_ = false;

  raw_ptr<Profile> profile_;

  std::unique_ptr<BirchCalendarProvider> calendar_provider_;

  std::unique_ptr<BirchFileSuggestProvider> file_suggest_provider_;

  std::unique_ptr<BirchRecentTabsProvider> recent_tabs_provider_;

  std::unique_ptr<BirchLastActiveProvider> last_active_provider_;

  std::unique_ptr<BirchMostVisitedProvider> most_visited_provider_;

  std::unique_ptr<BirchReleaseNotesProvider> release_notes_provider_;

  std::unique_ptr<BirchSelfShareProvider> self_share_provider_;

  std::unique_ptr<BirchLostMediaProvider> lost_media_provider_;

  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};

  std::unique_ptr<RefreshTokenWaiter> refresh_token_waiter_;

  // Task tracker for favicon requests.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // The test data provider is a separate member because it needs to be a
  // generic BirchDataProvider and `calendar_provider_` cannot be changed to
  // that type.
  raw_ptr<BirchDataProvider> calendar_provider_for_test_;

  // These are members for consistency with `calendar_provider_for_test`.
  raw_ptr<BirchDataProvider> file_suggest_provider_for_test_;
  raw_ptr<BirchDataProvider> recent_tabs_provider_for_test_;
  raw_ptr<BirchDataProvider> last_active_provider_for_test_;
  raw_ptr<BirchDataProvider> most_visited_provider_for_test_;
  raw_ptr<BirchDataProvider> self_share_provider_for_test_;
  raw_ptr<BirchDataProvider> lost_media_provider_for_test_;
  raw_ptr<BirchDataProvider> release_notes_provider_for_test_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_H_
