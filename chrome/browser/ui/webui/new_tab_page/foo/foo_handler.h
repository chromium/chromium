// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_FOO_FOO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_FOO_FOO_HANDLER_H_

#include "chrome/browser/ui/webui/new_tab_page/foo/foo.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Handles requests of dummy modules sent from JS.
class FooHandler : public foo::mojom::FooHandler {
 public:
  explicit FooHandler(mojo::PendingReceiver<foo::mojom::FooHandler> handler);
  ~FooHandler() override;

  // foo::mojom::FooHandler:
  void GetData(GetDataCallback callback) override;

 private:
  mojo::Receiver<foo::mojom::FooHandler> handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_FOO_FOO_HANDLER_H_
