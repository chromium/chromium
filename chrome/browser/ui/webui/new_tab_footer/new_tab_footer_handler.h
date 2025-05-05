// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

class NewTabFooterHandler : public new_tab_footer::mojom::NewTabFooterHandler {
 public:
  NewTabFooterHandler(
      mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>
          pending_handler,
      mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>
          pending_document,
      content::WebContents* web_contents);

  NewTabFooterHandler(const NewTabFooterHandler&) = delete;
  NewTabFooterHandler& operator=(const NewTabFooterHandler&) = delete;

  ~NewTabFooterHandler() override;

  // new_tab_footer::mojom::NewTabFooterHandler:
  void GetNtpExtensionAttribution(
      GetNtpExtensionAttributionCallback callback) override;

 private:
  const raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  mojo::Remote<new_tab_footer::mojom::NewTabFooterDocument> document_;
  mojo::Receiver<new_tab_footer::mojom::NewTabFooterHandler> handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_HANDLER_H_
