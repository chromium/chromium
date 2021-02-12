// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_H_

#include "base/callback.h"
#include "chrome/browser/video_tutorials/internal/tutorial_group.h"

namespace video_tutorials {

// Responsible for serving video tutorials and coordinating access with the
// network fetcher and the storage layer.
class TutorialManager {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using MultipleItemCallback = base::OnceCallback<void(std::vector<Tutorial>)>;
  using SingleItemCallback = base::OnceCallback<void(base::Optional<Tutorial>)>;

  // Loads video tutorials. Must be called again if the locale was changed by
  // the user.
  virtual void GetTutorials(MultipleItemCallback callback) = 0;

  // Called to retrieve the tutorial associated with |feature_type|.
  virtual void GetTutorial(FeatureType feature_type,
                           SingleItemCallback callback) = 0;

  // Returns a list of languages for which video tutorials are available.
  virtual const std::vector<std::string>& GetSupportedLanguages() = 0;

  // Returns a list of languages in which a given tutorial is available.
  virtual const std::vector<std::string>& GetAvailableLanguagesForTutorial(
      FeatureType feature_type) = 0;

  // Returns the preferred locale for the video tutorials.
  virtual base::Optional<std::string> GetPreferredLocale() = 0;

  // Sets the user preferred locale for watching the video tutorials. This
  // doesn't update the cached tutorials. GetTutorials must be called for the
  // new data to be reflected.
  virtual void SetPreferredLocale(const std::string& locale) = 0;

  // Saves a fresh set of video tutorials into database. Called after a network
  // fetch.
  virtual void SaveGroups(
      std::unique_ptr<std::vector<TutorialGroup>> groups) = 0;

  virtual ~TutorialManager() = default;

  TutorialManager(TutorialManager& other) = delete;
  TutorialManager& operator=(TutorialManager& other) = delete;

 protected:
  TutorialManager() = default;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_H_
