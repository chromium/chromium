// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DLP_INTERNALS_DLP_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DLP_INTERNALS_DLP_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/dlp_internals/dlp_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace policy {

// Concrete implementation of dlp_internals::mojom::PageHandler.
class DlpInternalsPageHandler : public dlp_internals::mojom::PageHandler {
 public:
  DlpInternalsPageHandler(
      mojo::PendingReceiver<dlp_internals::mojom::PageHandler> receiver,
      Profile* profile);

  DlpInternalsPageHandler(const DlpInternalsPageHandler&) = delete;
  DlpInternalsPageHandler& operator=(const DlpInternalsPageHandler&) = delete;

  ~DlpInternalsPageHandler() override;

 private:
  // dlp_internals::mojom::DlpInternalsPageHandler
  void GetClipboardDataSource(GetClipboardDataSourceCallback callback) override;
  void GetContentRestrictionsInfo(
      GetContentRestrictionsInfoCallback callback) override;

  mojo::Receiver<dlp_internals::mojom::PageHandler> receiver_;
  raw_ptr<Profile> profile_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_UI_WEBUI_DLP_INTERNALS_DLP_INTERNALS_PAGE_HANDLER_H_
