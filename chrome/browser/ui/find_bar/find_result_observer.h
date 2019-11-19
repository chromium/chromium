// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_RESULT_OBSERVER_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_RESULT_OBSERVER_H_

#include "base/observer_list_types.h"

class FindTabHelper;

namespace content {
class WebContents;
}

class FindResultObserver : public base::CheckedObserver {
 public:
  virtual void OnFindResultAvailable(content::WebContents* web_contents) = 0;

  virtual void OnFindTabHelperDestroyed(FindTabHelper* helper) {}
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_RESULT_OBSERVER_H_
