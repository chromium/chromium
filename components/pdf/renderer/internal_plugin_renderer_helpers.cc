// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/internal_plugin_renderer_helpers.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "components/pdf/renderer/pdf_internal_plugin_delegate.h"
#include "components/pdf/renderer/pdf_view_web_plugin_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/pdf_view_web_plugin.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace pdf {

bool IsPdfRenderer() {
  static const bool has_switch =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kPdfRenderer);
  return has_switch;
}

blink::WebPlugin* CreateInternalPlugin(
    blink::WebPluginParams params,
    content::RenderFrame* render_frame,
    std::unique_ptr<PdfInternalPluginDelegate> delegate) {
  // For a PDF plugin, `params.url` holds the plugin's stream URL. If `params`
  // contains an 'original-url' attribute, reset `params.url` with its original
  // URL value so that it can be used to determine the plugin's origin.
  for (size_t i = 0; i < params.attribute_names.size(); ++i) {
    if (params.attribute_names[i] == "original-url") {
      params.url = GURL(params.attribute_values[i].Utf16());
      break;
    }
  }

  // The in-process plugin should only be created if the parent frame's origin
  // was allowed to (externally) embed the internal plugin.
  blink::WebFrame* frame = render_frame->GetWebFrame();
  blink::WebFrame* parent_frame = frame->Parent();
  if (!parent_frame ||
      !delegate->IsAllowedOrigin(parent_frame->GetSecurityOrigin())) {
    return nullptr;
  }

  // Only create the in-process plugin within a PDF renderer.
  CHECK(IsPdfRenderer());

  // Origins allowed to embed the internal plugin are trusted (the PDF viewer
  // and Print Preview), and should never directly create the in-process plugin.
  // Likewise, they should not share a process with this frame.
  //
  // See crbug.com/1259635 and crbug.com/1261758 for examples of previous bugs.
  CHECK(!delegate->IsAllowedOrigin(frame->GetSecurityOrigin()));
  CHECK(parent_frame->IsWebRemoteFrame());

  mojo::AssociatedRemote<pdf::mojom::PdfHost> pdf_host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      pdf_host.BindNewEndpointAndPassReceiver());
  return new chrome_pdf::PdfViewWebPlugin(
      std::make_unique<PdfViewWebPluginClient>(render_frame),
      std::move(pdf_host), std::move(params));
}

}  // namespace pdf
