// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class NewTabPageHandler : public content::WebContentsObserver,
                          public new_tab_page::mojom::PageHandler {
 public:
  NewTabPageHandler(
      mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<new_tab_page::mojom::Page> pending_page);
  ~NewTabPageHandler() override;

 private:
  mojo::Remote<new_tab_page::mojom::Page> page_;

  mojo::Receiver<new_tab_page::mojom::PageHandler> receiver_;

  DISALLOW_COPY_AND_ASSIGN(NewTabPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_HANDLER_H_
