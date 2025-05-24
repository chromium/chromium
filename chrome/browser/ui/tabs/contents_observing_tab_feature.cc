// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"

#include "base/functional/bind.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

ContentsObservingTabFeature::ContentsObservingTabFeature(TabInterface& tab)
    : tab_(tab) {
  tab_subscription_ = tab.RegisterWillDiscardContents(base::BindRepeating(
      &ContentsObservingTabFeature::OnDiscardContents, base::Unretained(this)));
  Observe(tab.GetContents());
}

ContentsObservingTabFeature::~ContentsObservingTabFeature() = default;

void ContentsObservingTabFeature::OnDiscardContents(
    TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  CHECK_EQ(tab, &tab_.get());
  Observe(new_contents);
}

}  // namespace tabs
