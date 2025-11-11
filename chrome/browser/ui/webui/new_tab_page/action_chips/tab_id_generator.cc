// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"

#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "components/sessions/content/session_tab_helper.h"

SessionID TabIdGeneratorImpl::GenerateTabId(
    content::WebContents* contents) const {
  return sessions::SessionTabHelper::IdForTab(contents);
}

const TabIdGenerator* TabIdGeneratorImpl::Get() {
  static const base::NoDestructor<TabIdGeneratorImpl> kInstance;
  return kInstance.get();
}
