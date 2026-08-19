// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_NEW_WINDOW_PROXY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_NEW_WINDOW_PROXY_H_

#include "chrome/browser/ui/webui/ash/emoji/new_window_proxy.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class NewWindowProxy : public new_window_proxy::mojom::NewWindowProxy {
 public:
  explicit NewWindowProxy(
      mojo::PendingReceiver<new_window_proxy::mojom::NewWindowProxy> receiver);
  ~NewWindowProxy() override;

  void OpenUrl(const GURL& url) override;

 private:
  mojo::Receiver<new_window_proxy::mojom::NewWindowProxy> receiver_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_NEW_WINDOW_PROXY_H_
