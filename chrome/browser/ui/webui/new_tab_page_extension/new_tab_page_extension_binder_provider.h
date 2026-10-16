// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_BINDER_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_BINDER_PROVIDER_H_

#include "extensions/browser/extension_mojo_binder_registry.h"

// Exposes the Mojo interfaces needed by the NTP component extension's frames.
class NewTabPageExtensionBinderProvider
    : public extensions::ExtensionMojoBinderProvider {
 public:
  // Instantiates and registers this provider with
  // `ExtensionMojoBinderRegistry` for the given `browser_context`.
  static void Register(content::BrowserContext* browser_context);

  NewTabPageExtensionBinderProvider();
  ~NewTabPageExtensionBinderProvider() override;

  // extensions::ExtensionMojoBinderProvider:
  bool IsMojoJsEnabledForFrame() const override;
  void PopulateFrameBinders(
      mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const extensions::Extension& extension) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_BINDER_PROVIDER_H_
