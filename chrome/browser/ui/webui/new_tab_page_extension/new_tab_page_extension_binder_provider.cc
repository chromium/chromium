// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_binder_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_page_handler_factory.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"
#include "extensions/common/constants.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace {

void BindPageHandlerFactory(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory> receiver) {
  NewTabPageExtensionPageHandlerFactory::GetOrCreateForCurrentDocument(
      frame_host)
      ->Bind(std::move(receiver));
}

void BindSearchboxPageHandlerFactory(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory> receiver) {
  NewTabPageExtensionPageHandlerFactory::GetOrCreateForCurrentDocument(
      frame_host)
      ->Bind(std::move(receiver));
}

void BindColorChangeHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  ui::ColorChangeHandler::GetOrCreateForCurrentDocument(frame_host)
      ->Bind(std::move(receiver), /*allow_non_webui=*/true);
}

}  // namespace

// static
void NewTabPageExtensionBinderProvider::Register(
    content::BrowserContext* browser_context) {
  extensions::ExtensionMojoBinderRegistryFactory::GetOrCreateForBrowserContext(
      browser_context)
      ->RegisterProvider(base::PassKey<NewTabPageExtensionBinderProvider>(),
                         std::make_unique<NewTabPageExtensionBinderProvider>());
}

NewTabPageExtensionBinderProvider::NewTabPageExtensionBinderProvider()
    : ExtensionMojoBinderProvider(extension_misc::kNewTabPageExtensionId) {}

NewTabPageExtensionBinderProvider::~NewTabPageExtensionBinderProvider() =
    default;

bool NewTabPageExtensionBinderProvider::IsMojoJsEnabledForFrame() const {
  return true;
}

void NewTabPageExtensionBinderProvider::PopulateFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
    content::RenderFrameHost* render_frame_host,
    const extensions::Extension& extension) {
  binder_map.Add<new_tab_page::mojom::PageHandlerFactory>(
      base::BindRepeating(&BindPageHandlerFactory));
  binder_map.Add<searchbox::mojom::PageHandlerFactory>(
      base::BindRepeating(&BindSearchboxPageHandlerFactory));
  binder_map.Add<color_change_listener::mojom::PageHandler>(
      base::BindRepeating(&BindColorChangeHandler));
}
