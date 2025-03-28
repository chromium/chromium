// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HANDLER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class HistorySyncOptinHandler : public history_sync_optin::mojom::PageHandler {
 public:
  // Initializes the handler with the mojo handlers and the needed information
  // to be displayed as well as callbacks to the main native view.
  HistorySyncOptinHandler(
      mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver,
      mojo::PendingRemote<history_sync_optin::mojom::Page> page);
  ~HistorySyncOptinHandler() override;

  HistorySyncOptinHandler(const HistorySyncOptinHandler&) = delete;
  HistorySyncOptinHandler& operator=(const HistorySyncOptinHandler&) = delete;

 private:
  // Allows handling received messages from the web ui page.
  mojo::Receiver<history_sync_optin::mojom::PageHandler> receiver_;
  // Interface to send information to the web ui page.
  mojo::Remote<history_sync_optin::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HANDLER_H_
