// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_TAB_ID_GENERATOR_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_TAB_ID_GENERATOR_H_

#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

// Interface of the tab ID generator.
// This is to mock the free function used in production.
class TabIdGenerator {
 public:
  virtual ~TabIdGenerator() = default;

  virtual int32_t GenerateTabHandleId(const tabs::TabInterface* tab) const = 0;
};

class TabIdGeneratorImpl : public TabIdGenerator {
 public:
  int32_t GenerateTabHandleId(const tabs::TabInterface* tab) const override;
  static const TabIdGenerator* Get();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_TAB_ID_GENERATOR_H_
