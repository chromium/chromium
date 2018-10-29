// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_HELPER_H_
#define CHROME_BROWSER_UI_HATS_HATS_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class Profile;

// This is a browser side per tab helper that allows an entry trigger to
// launch Happiness Tracking Surveys (HaTS)
class HatsHelper : public content::WebContentsObserver,
                   public content::WebContentsUserData<HatsHelper> {
 public:
  ~HatsHelper() override;

 private:
  friend class content::WebContentsUserData<HatsHelper>;

  explicit HatsHelper(content::WebContents* web_contents);

  // Overridden from contents::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  Profile* profile() const;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(HatsHelper);
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_HELPER_H_
