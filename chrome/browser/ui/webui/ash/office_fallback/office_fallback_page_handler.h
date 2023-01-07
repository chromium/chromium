// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::office_fallback {

// Handles communication from the chrome://office_fallback renderer process to
// the browser process. Defines functions that the JS code running in the
// renderer process can invoke on the browser process.
class OfficeFallbackPageHandler : public mojom::PageHandler {
 public:
  using CloseCallback = base::OnceCallback<void(mojom::DialogChoice choice)>;
  OfficeFallbackPageHandler(
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
      CloseCallback callback);

  OfficeFallbackPageHandler(const OfficeFallbackPageHandler&) = delete;
  OfficeFallbackPageHandler& operator=(const OfficeFallbackPageHandler&) =
      delete;

  ~OfficeFallbackPageHandler() override;

  // mojom::PageHandler:
  void Close(mojom::DialogChoice choice) override;

 private:
  mojo::Receiver<PageHandler> receiver_;
  CloseCallback callback_;

  base::WeakPtrFactory<OfficeFallbackPageHandler> weak_ptr_factory_{this};
};

}  // namespace ash::office_fallback

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_PAGE_HANDLER_H_
