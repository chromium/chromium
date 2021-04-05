// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#ifndef CHROMEOS_COMPONENTS_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_DELEGATE_H_

// Handles calling |backdrop_wallpaper_handler| code in //chrome to pass to the
// Personalization App SWA.
class PersonalizationAppUiDelegate
    : public chromeos::personalization_app::mojom::WallpaperProvider {
 public:
  virtual void BindInterface(
      mojo::PendingReceiver<
          chromeos::personalization_app::mojom::WallpaperProvider>
          receiver) = 0;
};

#endif  // CHROMEOS_COMPONENTS_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_DELEGATE_H_
