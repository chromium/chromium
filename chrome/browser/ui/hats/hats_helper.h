// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_HELPER_H_
#define CHROME_BROWSER_UI_HATS_HATS_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class Profile;
class PerformanceControlsHatsService;

// This is a browser side per tab helper that allows an entry trigger to
// launch Happiness Tracking Surveys (HaTS)
class HatsHelper : public content::WebContentsObserver,
                   public content::WebContentsUserData<HatsHelper> {
 public:
  HatsHelper(const HatsHelper&) = delete;
  HatsHelper& operator=(const HatsHelper&) = delete;

  ~HatsHelper() override;

 private:
  friend class content::WebContentsUserData<HatsHelper>;

  explicit HatsHelper(content::WebContents* web_contents);

  raw_ptr<PerformanceControlsHatsService> performance_controls_hats_service_;

  // contents::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  Profile* profile() const;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_HELPER_H_
