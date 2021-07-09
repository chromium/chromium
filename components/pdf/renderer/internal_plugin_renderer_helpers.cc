// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/internal_plugin_renderer_helpers.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "pdf/buildflags.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_view_web_plugin.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "url/gurl.h"

namespace pdf {

blink::WebPlugin* MaybeCreateInternalPlugin(
    content::RenderFrame* render_frame,
    std::unique_ptr<chrome_pdf::PdfViewWebPlugin::PrintClient> print_client,
    blink::WebPluginParams& params) {
  // For a PDF plugin, `params.url` holds the plugin's stream URL. If `params`
  // contains an 'original-url' attribute, reset `params.url` with its original
  // URL value so that it can be used to determine the plugin's origin.
  for (size_t i = 0; i < params.attribute_names.size(); ++i) {
    if (params.attribute_names[i] == "original-url") {
      params.url = GURL(params.attribute_values[i].Utf16());
      break;
    }
  }

  if (!base::FeatureList::IsEnabled(chrome_pdf::features::kPdfUnseasoned)) {
    // Let the caller handle Pepper plugin creation.
    return nullptr;
  }

#if BUILDFLAG(ENABLE_PDF_UNSEASONED)
  // Create unseasoned PDF plugin directly, for development purposes.
  // TODO(crbug.com/1123621): Implement a more permanent solution once the new
  // PDF viewer process model is approved and in place.
  mojo::AssociatedRemote<pdf::mojom::PdfService> pdf_service_remote;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      pdf_service_remote.BindNewEndpointAndPassReceiver());
  return new chrome_pdf::PdfViewWebPlugin(std::move(pdf_service_remote),
                                          std::move(print_client), params);
#else   // !BUILDFLAG(ENABLE_PDF_UNSEASONED)
  return nullptr;
#endif  // BUILDFLAG(ENABLE_PDF_UNSEASONED)
}

}  // namespace pdf
