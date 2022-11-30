// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_TAB_HELPER_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_TAB_HELPER_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace video_tutorials {

class VideoTutorialTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<VideoTutorialTabHelper> {
 public:
  ~VideoTutorialTabHelper() override;
  VideoTutorialTabHelper(const VideoTutorialTabHelper&) = delete;
  VideoTutorialTabHelper& operator=(const VideoTutorialTabHelper&) = delete;

  // content::WebContentsObserver overrides.
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override;

 private:
  friend class content::WebContentsUserData<VideoTutorialTabHelper>;

  explicit VideoTutorialTabHelper(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_TAB_HELPER_H_
