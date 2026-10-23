// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_PAGE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_PAGE_HANDLER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class ActionChipsHandler;
class ComposeboxHandler;
class NewTabPageHandler;
class RealboxHandler;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

// Handles the NTP Mojo page handler factory requests from NTP component
// extension frames (`new_tab_page::mojom::PageHandlerFactory`,
// `searchbox::mojom::PageHandlerFactory`,
// `composebox::mojom::PageHandlerFactory` and
// `action_chips::mojom::ActionChipsHandlerFactory`), mirroring how
// `NewTabPageUI` implements them. Bound to the document lifecycle (via
// `content::DocumentUserData`), so it and the handlers it owns are scoped to
// the `RenderFrameHost`'s current document. Also owns per-document state shared
// by those handlers, such as the contextual search session.
class NewTabPageExtensionPageHandlerFactory
    : public content::DocumentUserData<NewTabPageExtensionPageHandlerFactory>,
      public new_tab_page::mojom::PageHandlerFactory,
      public searchbox::mojom::PageHandlerFactory,
      public composebox::mojom::PageHandlerFactory,
      public action_chips::mojom::ActionChipsHandlerFactory {
 public:
  NewTabPageExtensionPageHandlerFactory(
      const NewTabPageExtensionPageHandlerFactory&) = delete;
  NewTabPageExtensionPageHandlerFactory& operator=(
      const NewTabPageExtensionPageHandlerFactory&) = delete;
  ~NewTabPageExtensionPageHandlerFactory() override;

  void Bind(
      mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory> receiver);
  void Bind(
      mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory> receiver);
  void Bind(
      mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver);
  void Bind(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandlerFactory>
          receiver);

  // new_tab_page::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<new_tab_page::mojom::Page> page,
      mojo::PendingReceiver<new_tab_page::mojom::PageHandler> handler) override;

  // searchbox::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<searchbox::mojom::Page> page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler> handler) override;

  // composebox::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> handler,
      mojo::PendingRemote<searchbox::mojom::Page> searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler> searchbox_handler)
      override;

  // action_chips::mojom::ActionChipsHandlerFactory:
  void CreateActionChipsHandler(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> handler,
      mojo::PendingRemote<action_chips::mojom::Page> page) override;

 private:
  explicit NewTabPageExtensionPageHandlerFactory(content::RenderFrameHost* rfh);

  friend class content::DocumentUserData<NewTabPageExtensionPageHandlerFactory>;
  DOCUMENT_USER_DATA_KEY_DECL();

  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle();
  void ClearContextualSessionHandle();

  // Approximates the NTP navigation start time used for logging, since this
  // factory is created when the document first requests one of its interfaces.
  const base::Time navigation_start_time_;
  const base::TimeTicks navigation_start_time_ticks_;

  // The NTP component extension does not support modules. `NewTabPageHandler`
  // holds a raw pointer to this, so it must outlive `page_handler_`.
  const std::vector<ntp::ModuleIdDetail> module_id_details_;

  // Must outlive `realbox_handler_`, `composebox_handler_` and
  // `action_chips_handler_`.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      shared_session_handle_;

  mojo::Receiver<new_tab_page::mojom::PageHandlerFactory> page_receiver_{this};
  mojo::Receiver<searchbox::mojom::PageHandlerFactory> searchbox_receiver_{
      this};
  mojo::Receiver<composebox::mojom::PageHandlerFactory> composebox_receiver_{
      this};
  mojo::Receiver<action_chips::mojom::ActionChipsHandlerFactory>
      action_chips_receiver_{this};

  // Declared last so they are destroyed before the state they reference.
  std::unique_ptr<NewTabPageHandler> page_handler_;
  std::unique_ptr<RealboxHandler> realbox_handler_;
  std::unique_ptr<ComposeboxHandler> composebox_handler_;
  std::unique_ptr<ActionChipsHandler> action_chips_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_EXTENSION_NEW_TAB_PAGE_EXTENSION_PAGE_HANDLER_FACTORY_H_
