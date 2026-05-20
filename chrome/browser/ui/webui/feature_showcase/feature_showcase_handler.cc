// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/feature_showcase_handler.h"

#include <utility>

FeatureShowcaseHandler::FeatureShowcaseHandler(
    mojo::PendingReceiver<feature_showcase::mojom::FeatureShowcasePageHandler>
        receiver,
    base::OnceClosure finish_callback)
    : receiver_(this, std::move(receiver)),
      finish_callback_(std::move(finish_callback)) {}

FeatureShowcaseHandler::~FeatureShowcaseHandler() = default;

void FeatureShowcaseHandler::FinishFeatureShowcase() {
  if (finish_callback_) {
    std::move(finish_callback_).Run();
  }
}
