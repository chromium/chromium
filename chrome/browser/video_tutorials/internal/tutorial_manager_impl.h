// Copyright 2020 The Chromium Authors
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
#include "chrome/browser/video_tutorials/internal/proto_conversions.h"
#include "chrome/browser/video_tutorials/internal/store.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace video_tutorials {

class TutorialManagerImpl : public TutorialManager {
 public:
  using TutorialStore = Store<proto::VideoTutorialGroups>;
  TutorialManagerImpl(std::unique_ptr<TutorialStore> store, PrefService* prefs);
  ~TutorialManagerImpl() override;

 private:
  // TutorialManager implementation.
  void Initialize(SuccessCallback callback) override;
  bool IsInitialized() override;
  void GetTutorials(MultipleItemCallback callback) override;
  void GetTutorial(FeatureType feature_type,
                   SingleItemCallback callback) override;
  std::vector<std::string> GetSupportedLanguages() override;
  const std::vector<std::string>& GetAvailableLanguagesForTutorial(
      FeatureType feature_type) override;
  absl::optional<std::string> GetPreferredLocale() override;
  void SetPreferredLocale(const std::string& locale) override;
  std::string GetTextLocale() override;
  void SaveGroups(std::unique_ptr<proto::VideoTutorialGroups> groups) override;

  void OnDataLoaded(SuccessCallback callback,
                    bool success,
                    std::unique_ptr<proto::VideoTutorialGroups> loaded_groups);
  void OnGroupsSaved(bool success);

  // The underlying persistence store.
  std::unique_ptr<TutorialStore> store_;

  // List of supported languages per tutorial.
  std::map<FeatureType, std::vector<std::string>> languages_for_tutorials_;

  // Cached copy of the DB. Loaded on the first call to GetTutorials.
  std::unique_ptr<proto::VideoTutorialGroups> tutorial_groups_;

  // The initialization status of the database.
  bool initialized_{false};

  base::WeakPtrFactory<TutorialManagerImpl> weak_ptr_factory_{this};
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_IMPL_H_
