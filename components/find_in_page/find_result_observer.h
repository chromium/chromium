// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FIND_IN_PAGE_FIND_RESULT_OBSERVER_H_
#define COMPONENTS_FIND_IN_PAGE_FIND_RESULT_OBSERVER_H_

#include "base/observer_list_types.h"

namespace content {
class WebContents;
}

namespace find_in_page {

class FindTabHelper;

class FindResultObserver : public base::CheckedObserver {
 public:
  virtual void OnFindResultAvailable(content::WebContents* web_contents) = 0;

  virtual void OnFindEmptyText(content::WebContents* web_contents) {}
  virtual void OnFindTabHelperDestroyed(FindTabHelper* helper) {}
};

}  // namespace find_in_page

#endif  // COMPONENTS_FIND_IN_PAGE_FIND_RESULT_OBSERVER_H_
