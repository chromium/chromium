// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LENS_LENS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_LENS_LENS_PAGE_HANDLER_H_

#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace lens {

class LensPageHandler : public lens::mojom::LensPageHandler {
 public:
  explicit LensPageHandler(
      mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
      mojo::PendingRemote<lens::mojom::LensPage> page);
  LensPageHandler(const LensPageHandler&) = delete;
  LensPageHandler& operator=(const LensPageHandler&) = delete;
  ~LensPageHandler() override;

 private:
  mojo::Receiver<lens::mojom::LensPageHandler> receiver_;
  mojo::Remote<lens::mojom::LensPage> page_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_WEBUI_LENS_LENS_PAGE_HANDLER_H_
