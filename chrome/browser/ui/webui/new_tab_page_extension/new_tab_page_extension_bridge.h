// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_BRIDGE_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_BRIDGE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

// Per-profile service that registers the NTP component extension's config and
// Mojo binder providers on construction.
class NewTabPageExtensionBridge : public KeyedService {
 public:
  explicit NewTabPageExtensionBridge(Profile* profile);
  NewTabPageExtensionBridge(const NewTabPageExtensionBridge&) = delete;
  NewTabPageExtensionBridge& operator=(const NewTabPageExtensionBridge&) =
      delete;
  ~NewTabPageExtensionBridge() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_BRIDGE_H_
