// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_H_

#include "base/callback.h"
#include "chrome/browser/video_tutorials/internal/tutorial_group.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace video_tutorials {
namespace proto {
class VideoTutorialGroups;
}  // namespace proto

// Responsible for serving video tutorials and coordinating access with the
// network fetcher and the storage layer.
class TutorialManager {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using MultipleItemCallback = base::OnceCallback<void(std::vector<Tutorial>)>;
  using SingleItemCallback = base::OnceCallback<void(absl::optional<Tutorial>)>;

  // Called to initialize the DB. Any other method calls to this class will be
  // cached and invoked after the initialization is completed.
  virtual void Initialize(SuccessCallback callback) = 0;

  // Returns whether the initialization is complete.
  virtual bool IsInitialized() = 0;

  // Called to return the list of video tutorials in the user's preferred
  // language. Must be invoked after preferred language is changed. If preferred
  // language is not set, it returns the tutorials in the first available
  // language.
  // This method also loads the data from DB for the first time. Hence every
  // other method must be invoked after a call to this one.
  virtual void GetTutorials(MultipleItemCallback callback) = 0;

  // Called to retrieve the tutorial associated with |feature_type|.
  virtual void GetTutorial(FeatureType feature_type,
                           SingleItemCallback callback) = 0;

  // Returns a list of languages for which video tutorials are available.
  virtual std::vector<std::string> GetSupportedLanguages() = 0;

  // Returns a list of languages in which a given tutorial is available.
  virtual const std::vector<std::string>& GetAvailableLanguagesForTutorial(
      FeatureType feature_type) = 0;

  // Returns the preferred locale for watching the video tutorials.
  virtual absl::optional<std::string> GetPreferredLocale() = 0;

  // Sets the user preferred locale for watching the video tutorials. This
  // doesn't update the cached tutorials. GetTutorials must be called for the
  // new data to be reflected.
  virtual void SetPreferredLocale(const std::string& locale) = 0;

  // Returns the locale used for showing the video card title text.
  virtual std::string GetTextLocale() = 0;

  // Saves a fresh set of video tutorials into database. Called after a network
  // fetch.
  virtual void SaveGroups(
      std::unique_ptr<proto::VideoTutorialGroups> groups) = 0;

  virtual ~TutorialManager() = default;

  TutorialManager(TutorialManager& other) = delete;
  TutorialManager& operator=(TutorialManager& other) = delete;

 protected:
  TutorialManager() = default;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_H_
