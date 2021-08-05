// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ENTERPRISE_CASTING_ENTERPRISE_CASTING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ENTERPRISE_CASTING_ENTERPRISE_CASTING_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/webui/chromeos/enterprise_casting/enterprise_casting.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class EnterpriseCastingHandler : public enterprise_casting::mojom::PageHandler {
 public:
  EnterpriseCastingHandler(
      mojo::PendingReceiver<enterprise_casting::mojom::PageHandler>
          page_handler,
      mojo::PendingRemote<enterprise_casting::mojom::Page> page);
  ~EnterpriseCastingHandler() override;

  // enterprise_casting::mojom::PageHandler :
  void UpdatePin() override;

 private:
  mojo::Remote<enterprise_casting::mojom::Page> page_;
  mojo::Receiver<enterprise_casting::mojom::PageHandler> receiver_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ENTERPRISE_CASTING_ENTERPRISE_CASTING_HANDLER_H_
