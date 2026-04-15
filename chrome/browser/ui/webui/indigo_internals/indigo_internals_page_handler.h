// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INDIGO_INTERNALS_INDIGO_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INDIGO_INTERNALS_INDIGO_INTERNALS_PAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/ui/webui/indigo_internals/indigo_internals.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

class IndigoInternalsPageHandler : public indigo_internals::mojom::PageHandler {
 public:
  IndigoInternalsPageHandler(
      mojo::PendingReceiver<indigo_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<indigo_internals::mojom::Page> page,
      Profile* profile);

  IndigoInternalsPageHandler(const IndigoInternalsPageHandler&) = delete;
  IndigoInternalsPageHandler& operator=(const IndigoInternalsPageHandler&) =
      delete;

  ~IndigoInternalsPageHandler() override;

  // indigo_internals::mojom::PageHandler:
  void GetLocalEligibility(GetLocalEligibilityCallback callback) override;
  void GetCombinedEligibility(GetCombinedEligibilityCallback callback) override;
  void InvalidateRemoteEligibility() override;

 private:
  void OnLocalEligibilityChanged(indigo::LocalEligibility status);

  mojo::Receiver<indigo_internals::mojom::PageHandler> receiver_;
  mojo::Remote<indigo_internals::mojom::Page> page_;
  raw_ptr<Profile> profile_;
  base::CallbackListSubscription subscription_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INDIGO_INTERNALS_INDIGO_INTERNALS_PAGE_HANDLER_H_
