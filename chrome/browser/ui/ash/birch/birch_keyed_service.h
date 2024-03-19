// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_H_

#include <memory>

#include "ash/birch/birch_client.h"
#include "ash/shell_observer.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {

class BirchCalendarProvider;
class BirchFileSuggestProvider;
class BirchRecentTabsProvider;
class BirchReleaseNotesProvider;
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
  BirchDataProvider* GetReleaseNotesProvider() override;
  void WaitForRefreshTokens(base::OnceClosure callback) override;
  base::FilePath GetRemovedItemsFilePath() override;

 private:
  void ShutdownBirch();

  // Whether shutdown of BirchKeyedService has already begun.
  bool is_shutdown_ = false;

  raw_ptr<Profile> profile_;

  std::unique_ptr<BirchCalendarProvider> calendar_provider_;

  std::unique_ptr<BirchFileSuggestProvider> file_suggest_provider_;

  std::unique_ptr<BirchRecentTabsProvider> recent_tabs_provider_;

  std::unique_ptr<BirchReleaseNotesProvider> release_notes_provider_;

  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};

  std::unique_ptr<RefreshTokenWaiter> refresh_token_waiter_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_H_
