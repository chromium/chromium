// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_bridge.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_binder_provider.h"
#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_config_provider.h"

NewTabPageExtensionBridge::NewTabPageExtensionBridge(Profile* profile) {
  NewTabPageExtensionConfigProvider::Register(profile);
  NewTabPageExtensionBinderProvider::Register(profile);
}

NewTabPageExtensionBridge::~NewTabPageExtensionBridge() = default;
