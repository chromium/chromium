// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_STARTED_ANIMATION_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_STARTED_ANIMATION_H_

namespace content {
class WebContents;
}

class DownloadStartedAnimation {
 public:
  DownloadStartedAnimation(const DownloadStartedAnimation&) = delete;
  DownloadStartedAnimation& operator=(const DownloadStartedAnimation&) = delete;

  static void Show(content::WebContents* web_contents);

 private:
  DownloadStartedAnimation() {}
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_STARTED_ANIMATION_H_
