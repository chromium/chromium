// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_FAKE_TAB_ID_GENERATOR_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_FAKE_TAB_ID_GENERATOR_H_

#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "components/tabs/public/tab_interface.h"

class FakeTabIdGenerator : public TabIdGenerator {
 public:
  int32_t GenerateTabHandleId(
      const tabs::TabInterface* contents) const override;
  static const FakeTabIdGenerator* Get();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_FAKE_TAB_ID_GENERATOR_H_
