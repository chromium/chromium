// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_HANDLER_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/feature_showcase/feature_showcase.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class FeatureShowcaseHandler
    : public feature_showcase::mojom::FeatureShowcasePageHandler {
 public:
  FeatureShowcaseHandler(
      mojo::PendingReceiver<feature_showcase::mojom::FeatureShowcasePageHandler>
          receiver,
      base::OnceClosure finish_callback);
  FeatureShowcaseHandler(const FeatureShowcaseHandler&) = delete;
  FeatureShowcaseHandler& operator=(const FeatureShowcaseHandler&) = delete;
  ~FeatureShowcaseHandler() override;

  // feature_showcase::mojom::FeatureShowcasePageHandler:
  void FinishFeatureShowcase() override;

 private:
  mojo::Receiver<feature_showcase::mojom::FeatureShowcasePageHandler> receiver_;
  base::OnceClosure finish_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_HANDLER_H_
