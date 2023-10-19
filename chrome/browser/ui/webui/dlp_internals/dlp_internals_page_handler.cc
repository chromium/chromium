// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dlp_internals/dlp_internals_page_handler.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/dlp_internals/dlp_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/clipboard/clipboard.h"

namespace policy {

DlpInternalsPageHandler::DlpInternalsPageHandler(
    mojo::PendingReceiver<dlp_internals::mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {
  DCHECK(profile_);
}

DlpInternalsPageHandler::~DlpInternalsPageHandler() = default;

void DlpInternalsPageHandler::GetClipboardDataSource(
    GetClipboardDataSourceCallback callback) {
  const auto* source = ui::Clipboard::GetForCurrentThread()->GetSource(
      ui::ClipboardBuffer::kCopyPaste);
  if (!source) {
    std::move(callback).Run(std::move(nullptr));
    return;
  }

  auto mojom_source = dlp_internals::mojom::DataTransferEndpoint::New();
  switch (source->type()) {
    case ui::EndpointType::kDefault:
      mojom_source->type = dlp_internals::mojom::EndpointType::kDefault;
      break;

    case ui::EndpointType::kUrl:
      mojom_source->type = dlp_internals::mojom::EndpointType::kUrl;
      break;

    case ui::EndpointType::kClipboardHistory:
      mojom_source->type =
          dlp_internals::mojom::EndpointType::kClipboardHistory;
      break;

    case ui::EndpointType::kUnknownVm:
      mojom_source->type = dlp_internals::mojom::EndpointType::kUnknownVm;
      break;

    case ui::EndpointType::kArc:
      mojom_source->type = dlp_internals::mojom::EndpointType::kArc;
      break;

    case ui::EndpointType::kBorealis:
      mojom_source->type = dlp_internals::mojom::EndpointType::kBorealis;
      break;

    case ui::EndpointType::kCrostini:
      mojom_source->type = dlp_internals::mojom::EndpointType::kCrostini;
      break;

    case ui::EndpointType::kPluginVm:
      mojom_source->type = dlp_internals::mojom::EndpointType::kPluginVm;
      break;

    case ui::EndpointType::kLacros:
      mojom_source->type = dlp_internals::mojom::EndpointType::kLacros;
      break;
  }

  if (source->IsUrlType()) {
    mojom_source->url = source->GetURL()->spec();
  }

  std::move(callback).Run(std::move(mojom_source));
}

}  // namespace policy
