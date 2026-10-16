// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_bridge.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_binder_provider.h"
#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_config_provider.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#include "content/public/browser/url_data_source.h"

NewTabPageExtensionBridge::NewTabPageExtensionBridge(Profile* profile) {
  NewTabPageExtensionConfigProvider::Register(profile);
  NewTabPageExtensionBinderProvider::Register(profile);
  // chrome://image is otherwise only registered when a WebUI that uses it
  // (e.g. chrome://new-tab-page) is created.
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
}

NewTabPageExtensionBridge::~NewTabPageExtensionBridge() = default;
