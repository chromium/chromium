// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_SERVICE_IMPL_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_SERVICE_IMPL_H_

#include <deque>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/video_tutorials/internal/tutorial_fetcher.h"
#include "chrome/browser/video_tutorials/internal/tutorial_manager.h"
#include "chrome/browser/video_tutorials/video_tutorial_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace video_tutorials {

class TutorialServiceImpl : public VideoTutorialService {
 public:
  TutorialServiceImpl(std::unique_ptr<TutorialManager> tutorial_manager,
                      std::unique_ptr<TutorialFetcher> tutorial_fetcher,
                      PrefService* pref_service);
  ~TutorialServiceImpl() override;

  // TutorialService implementation.
  void GetTutorials(MultipleItemCallback callback) override;
  void GetTutorial(FeatureType feature_type,
                   SingleItemCallback callback) override;
  std::vector<std::string> GetSupportedLanguages() override;
  const std::vector<std::string>& GetAvailableLanguagesForTutorial(
      FeatureType feature_type) override;
  absl::optional<std::string> GetPreferredLocale() override;
  void SetPreferredLocale(const std::string& locale) override;

 private:
  // Called at service startup to determine if a network fetch is necessary
  // based on the last fetch timestamp.
  void StartFetchIfNecessary();
  void OnFetchFinished(bool success,
                       std::unique_ptr<std::string> response_body);
  void OnManagerInitialized(bool success);
  void OnAcceptLanguagesChanged();
  void MaybeCacheApiCall(base::OnceClosure api_call);
  void FlushCachedApiCalls();

  // Manages in memory tutorial metadata and coordinates with TutorialStore.
  std::unique_ptr<TutorialManager> tutorial_manager_;

  // Fetcher to execute download jobs from Google server.
  std::unique_ptr<TutorialFetcher> tutorial_fetcher_;

  // PrefService.
  raw_ptr<PrefService> pref_service_;

  // Listens to accept languages changes.
  PrefChangeRegistrar pref_change_registrar_;

  // Cached API calls before initialization.
  std::deque<base::OnceClosure> cached_api_calls_;

  base::WeakPtrFactory<TutorialServiceImpl> weak_ptr_factory_{this};
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_SERVICE_IMPL_H_
