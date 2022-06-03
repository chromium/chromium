// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/federated_learning/floc_internals_page_handler.h"

#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/profiles/profile.h"

FlocInternalsPageHandler::FlocInternalsPageHandler(
    Profile* profile,
    mojo::PendingReceiver<federated_learning::mojom::PageHandler> receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {}

FlocInternalsPageHandler::~FlocInternalsPageHandler() = default;

void FlocInternalsPageHandler::GetFlocStatus(
    federated_learning::mojom::PageHandler::GetFlocStatusCallback callback) {
  federated_learning::FlocIdProvider* floc_id_provider =
      federated_learning::FlocIdProviderFactory::GetForProfile(profile_);

  if (!floc_id_provider) {
    std::move(callback).Run(federated_learning::mojom::WebUIFlocStatus::New());
    return;
  }

  std::move(callback).Run(floc_id_provider->GetFlocStatusForWebUi());
}
