// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEDERATED_LEARNING_FLOC_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEDERATED_LEARNING_FLOC_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/federated_learning/floc_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;

// Implements the mojo endpoint for the FLoC WebUI which proxies calls to the
// FlocIdProvider to get information about relevant floc status. Owned by
// FlocInternalsUI.
class FlocInternalsPageHandler : public federated_learning::mojom::PageHandler {
 public:
  FlocInternalsPageHandler(
      Profile* profile,
      mojo::PendingReceiver<federated_learning::mojom::PageHandler> receiver);
  FlocInternalsPageHandler(const FlocInternalsPageHandler&) = delete;
  FlocInternalsPageHandler& operator=(const FlocInternalsPageHandler&) = delete;
  ~FlocInternalsPageHandler() override;

  // federated_learning::mojom::PageHandler overrides:
  void GetFlocStatus(
      federated_learning::mojom::PageHandler::GetFlocStatusCallback callback)
      override;

 private:
  raw_ptr<Profile> profile_;

  mojo::Receiver<federated_learning::mojom::PageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEDERATED_LEARNING_FLOC_INTERNALS_PAGE_HANDLER_H_
