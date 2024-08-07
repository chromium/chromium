// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

// Provides an interface for decluttering tabs.
class TabDeclutterService : public KeyedService {
 public:
  TabDeclutterService();
  TabDeclutterService(const TabDeclutterService&) = delete;
  TabDeclutterService& operator=(const TabDeclutterService& other) = delete;
  ~TabDeclutterService() override;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_SERVICE_H_
