// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INFOBAR_INTERNALS_INFOBAR_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INFOBAR_INTERNALS_INFOBAR_INTERNALS_HANDLER_H_

#include "chrome/browser/ui/webui/infobar_internals/infobar_internals.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// The handler for Javascript messages related to the "infobar-internals" page.
class InfoBarInternalsHandler final
    : public infobar_internals::mojom::PageHandler {
 public:
  explicit InfoBarInternalsHandler(
      mojo::PendingReceiver<infobar_internals::mojom::PageHandler> receiver);

  InfoBarInternalsHandler(const InfoBarInternalsHandler&) = delete;
  InfoBarInternalsHandler& operator=(const InfoBarInternalsHandler&) = delete;

  ~InfoBarInternalsHandler() override;

  // infobar_internals::mojom::PageHandler:
  void GetInfoBars(GetInfoBarsCallback callback) override;
  void TriggerInfoBar(infobar_internals::mojom::InfoBarType type,
                      TriggerInfoBarCallback callback) override;

 private:
  // Returns true on success, false if the requested type is unsupported or the
  // triggering fail.
  bool TriggerInfoBarInternal(infobar_internals::mojom::InfoBarType type);

  mojo::Receiver<infobar_internals::mojom::PageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INFOBAR_INTERNALS_INFOBAR_INTERNALS_HANDLER_H_
