// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_readiness_checker.h"

#include "base/no_destructor.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"

bool TabReadinessCheckerImpl::IsReadyForActionChipsRetrieval(
    const content::WebContents* web_contents) const {
  // The expression used below returns false when the following occur to a tab
  // opening NTP:
  // - It is dragged out of a window. This results in a crash when we touch
  //   its tab strip model.
  // - It is no longer an active tab.
  // We use the current implementation to address the former quickly. If
  // returning false in the latter case becomes an issue, a better way to tell
  // the former should be considered.
  return tabs::TabInterface::GetFromContents(web_contents)->IsActivated();
}

const TabReadinessChecker* TabReadinessCheckerImpl::Get() {
  static const base::NoDestructor<TabReadinessCheckerImpl> kInstance;
  return kInstance.get();
}
