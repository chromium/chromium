// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_GLIC_GLIC_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_GLIC_GLIC_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace glic {

class GlicPageHandler : public glic::mojom::PageHandler {
 public:
  GlicPageHandler(mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
                  mojo::PendingRemote<glic::mojom::Page> page);

  GlicPageHandler(const GlicPageHandler&) = delete;
  GlicPageHandler& operator=(const GlicPageHandler&) = delete;

  ~GlicPageHandler() override;

  void GetChromeVersion(GetChromeVersionCallback callback) override;

 private:
  mojo::Receiver<glic::mojom::PageHandler> receiver_;
  mojo::Remote<glic::mojom::Page> page_;
  base::WeakPtrFactory<GlicPageHandler> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_UI_WEBUI_GLIC_GLIC_PAGE_HANDLER_H_
