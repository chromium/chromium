// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_GOOGLE_LENS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_GOOGLE_LENS_HANDLER_H_

#include "chrome/browser/ui/webui/feature_showcase/google_lens.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

class GoogleLensHandler
    : public feature_showcase::mojom::GoogleLensPageHandler {
 public:
  GoogleLensHandler(
      mojo::PendingReceiver<feature_showcase::mojom::GoogleLensPageHandler>
          receiver,
      Profile* profile);
  GoogleLensHandler(const GoogleLensHandler&) = delete;
  GoogleLensHandler& operator=(const GoogleLensHandler&) = delete;
  ~GoogleLensHandler() override;

  // feature_showcase::mojom::GoogleLensPageHandler:
  void EnableGoogleLens() override;
  void SkipGoogleLens() override;

 private:
  mojo::Receiver<feature_showcase::mojom::GoogleLensPageHandler> receiver_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_GOOGLE_LENS_HANDLER_H_
