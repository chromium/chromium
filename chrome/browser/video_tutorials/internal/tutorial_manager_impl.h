// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_IMPL_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_IMPL_H_

#include "chrome/browser/video_tutorials/internal/tutorial_manager.h"

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/video_tutorials/internal/store.h"

class PrefService;

namespace video_tutorials {

class TutorialManagerImpl : public TutorialManager {
 public:
  using TutorialStore = Store<TutorialGroup>;
  TutorialManagerImpl(std::unique_ptr<TutorialStore> store, PrefService* prefs);
  ~TutorialManagerImpl() override;

 private:
  // TutorialManager implementation.
  void GetTutorials(MultipleItemCallback callback) override;
  void GetTutorial(FeatureType feature_type,
                   SingleItemCallback callback) override;
  const std::vector<std::string>& GetSupportedLanguages() override;
  const std::vector<std::string>& GetAvailableLanguagesForTutorial(
      FeatureType feature_type) override;
  base::Optional<std::string> GetPreferredLocale() override;
  void SetPreferredLocale(const std::string& locale) override;
  void SaveGroups(std::unique_ptr<std::vector<TutorialGroup>> groups) override;

  void Initialize();
  void OnInitCompleted(bool success);
  void OnInitialDataLoaded(
      bool success,
      std::unique_ptr<std::vector<TutorialGroup>> all_groups);
  void MaybeCacheApiCall(base::OnceClosure api_call);
  void OnTutorialsLoaded(
      MultipleItemCallback callback,
      bool success,
      std::unique_ptr<std::vector<TutorialGroup>> loaded_groups);
  void RunSingleItemCallback(SingleItemCallback callback,
                             FeatureType feature_type,
                             std::vector<Tutorial> tutorials_excluding_summary);

  std::unique_ptr<TutorialStore> store_;
  PrefService* prefs_;

  // List of languages for which we have tutorials.
  std::vector<std::string> supported_languages_;

  // List of supported languages per tutorial.
  std::map<FeatureType, std::vector<std::string>> languages_for_tutorials_;

  // We only keep the tutorials for the preferred locale.
  base::Optional<TutorialGroup> tutorial_group_;

  // The initialization result of the database.
  base::Optional<bool> init_success_;

  // Caches the API calls in case initialization is not completed.
  std::deque<base::OnceClosure> cached_api_calls_;

  base::WeakPtrFactory<TutorialManagerImpl> weak_ptr_factory_{this};
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_IMPL_H_
