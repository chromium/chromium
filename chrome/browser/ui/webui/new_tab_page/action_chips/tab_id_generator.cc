// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"

#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "components/tabs/public/tab_interface.h"

using ::tabs::TabInterface;

int32_t TabIdGeneratorImpl::GenerateTabHandleId(const TabInterface* tab) const {
  if (tab == nullptr) {
    return tabs::TabHandle::NullValue;
  }
  return tab->GetHandle().raw_value();
}

const TabIdGenerator* TabIdGeneratorImpl::Get() {
  static const base::NoDestructor<TabIdGeneratorImpl> kInstance;
  return kInstance.get();
}
