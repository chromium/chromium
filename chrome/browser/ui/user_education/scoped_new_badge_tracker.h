// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_SCOPED_NEW_BADGE_TRACKER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_SCOPED_NEW_BADGE_TRACKER_H_

#include "components/user_education/common/scoped_new_badge_tracker_base.h"

namespace content {
class BrowserContext;
}

class Browser;

// Implementation of ScopedNewBadgeTrackerBase that allows you to pass a
// Browser or Profile to the constructor.
//
// See documentation on ScopedNewBadgeTrackerBase.
class ScopedNewBadgeTracker : public user_education::ScopedNewBadgeTrackerBase {
 public:
  // Constructs a scoped tracker for browser with |profile|.
  explicit ScopedNewBadgeTracker(content::BrowserContext* profile);

  // Constructs a scoped tracker for browser from |browser|.
  explicit ScopedNewBadgeTracker(Browser* browser);
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_SCOPED_NEW_BADGE_TRACKER_H_
