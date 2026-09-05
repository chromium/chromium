// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BINDER_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BINDER_PROVIDER_H_

#include "extensions/browser/extension_mojo_binder_registry.h"

class AimEligibilityExtensionBinderProvider
    : public extensions::ExtensionMojoBinderProvider {
 public:
  // Instantiates and registers this provider with
  // `ExtensionMojoBinderRegistry` for the given `browser_context`.
  static void Register(content::BrowserContext* browser_context);

  AimEligibilityExtensionBinderProvider();
  ~AimEligibilityExtensionBinderProvider() override;

  // extensions::ExtensionMojoBinderProvider:
  bool IsMojoJsEnabledForFrame() const override;
  bool IsMojoJsEnabledForServiceWorker() const override;
  void PopulateFrameBinders(
      mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const extensions::Extension& extension) override;
  void PopulateServiceWorkerBinders(
      mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>&
          binder_map,
      content::BrowserContext* browser_context,
      const extensions::Extension& extension) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BINDER_PROVIDER_H_
