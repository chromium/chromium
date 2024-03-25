// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {

class TabModel;

class TabModelObserver : public base::CheckedObserver {
 public:
  // This method is called when the contents of a tab is about to be removed.
  // This can happen if the tab is in the background and we discard the web
  // contents to free resources.
  virtual void WillRemoveContents(TabModel* tab,
                                  content::WebContents* contents) = 0;

  // This method is called after a tab transitions from nullptr to a
  // non-nullptr contents.
  virtual void DidAddContents(TabModel* tab,
                              content::WebContents* contents) = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_MODEL_OBSERVER_H_
