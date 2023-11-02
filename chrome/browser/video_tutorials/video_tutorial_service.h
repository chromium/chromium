// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_SERVICE_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/supports_user_data.h"
#include "chrome/browser/video_tutorials/tutorial.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace video_tutorials {

using TutorialList = std::vector<Tutorial>;
using MultipleItemCallback = base::OnceCallback<void(std::vector<Tutorial>)>;
using SingleItemCallback = base::OnceCallback<void(absl::optional<Tutorial>)>;

// The central class on chrome client responsible for managing, storing, and
// serving video tutorials in chrome.
class VideoTutorialService : public KeyedService,
                             public base::SupportsUserData {
 public:
  // Called to retrieve all the tutorials.
  virtual void GetTutorials(MultipleItemCallback callback) = 0;

  // Called to retrieve the tutorial associated with |feature_type|.
  virtual void GetTutorial(FeatureType feature_type,
                           SingleItemCallback callback) = 0;

  // Called to retrieve all the supported locales.
  virtual std::vector<std::string> GetSupportedLanguages() = 0;

  // Returns a list of languages in which a given tutorial is available.
  virtual const std::vector<std::string>& GetAvailableLanguagesForTutorial(
      FeatureType feature_type) = 0;

  // Called to retrieve the preferred locale, if it is set by the user.
  virtual absl::optional<std::string> GetPreferredLocale() = 0;

  // Called to set the preferred locale.
  virtual void SetPreferredLocale(const std::string& locale) = 0;

  VideoTutorialService() = default;
  ~VideoTutorialService() override = default;

  VideoTutorialService(const VideoTutorialService&) = delete;
  VideoTutorialService& operator=(const VideoTutorialService&) = delete;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_SERVICE_H_
