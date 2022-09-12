// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_PAGE_HANDLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload.mojom-shared.h"
#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos::cloud_upload {

// Handles communication from the chrome://cloud-upload renderer process to
// the browser process exposing various methods for the JS to invoke.
class CloudUploadPageHandler
    : public chromeos::cloud_upload::mojom::PageHandler {
 public:
  using RespondAndCloseCallback =
      base::OnceCallback<void(mojom::UserAction action)>;
  explicit CloudUploadPageHandler(
      mojo::PendingReceiver<chromeos::cloud_upload::mojom::PageHandler>
          pending_page_handler,
      RespondAndCloseCallback callback);

  CloudUploadPageHandler(const CloudUploadPageHandler&) = delete;
  CloudUploadPageHandler& operator=(const CloudUploadPageHandler&) = delete;

  ~CloudUploadPageHandler() override;

  // chromeos::cloud_upload::mojom::PageHandler:
  void RespondAndClose(mojom::UserAction action) override;

 private:
  mojo::Receiver<chromeos::cloud_upload::mojom::PageHandler> receiver_;
  RespondAndCloseCallback callback_;

  base::WeakPtrFactory<CloudUploadPageHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_PAGE_HANDLER_H_
