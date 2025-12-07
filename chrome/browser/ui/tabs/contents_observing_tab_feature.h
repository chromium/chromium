// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_CONTENTS_OBSERVING_TAB_FEATURE_H_
#define CHROME_BROWSER_UI_TABS_CONTENTS_OBSERVING_TAB_FEATURE_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"

namespace tabs {

// Base class for tab features that need to track the state of the WebContents
// in the tab, possibly across discards. This is likely to be a common pattern
// among tab helpers that care about the tab's WebContents, so is provided as a
// convenience.
class ContentsObservingTabFeature : public content::WebContentsObserver {
 public:
  explicit ContentsObservingTabFeature(TabInterface& tab);
  ~ContentsObservingTabFeature() override;

  TabInterface& tab() { return *tab_; }
  const TabInterface& tab() const { return *tab_; }

 protected:
  // This is called when the tab contents are discarded; the `new_contents` will
  // be observed after this point.
  virtual void OnDiscardContents(TabInterface* tab,
                                 content::WebContents* old_contents,
                                 content::WebContents* new_contents);

 private:
  const raw_ref<TabInterface> tab_;
  base::CallbackListSubscription tab_subscription_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_CONTENTS_OBSERVING_TAB_FEATURE_H_
