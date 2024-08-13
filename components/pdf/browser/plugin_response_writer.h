// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PLUGIN_RESPONSE_WRITER_H_
#define COMPONENTS_PDF_BROWSER_PLUGIN_RESPONSE_WRITER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace mojo {
class DataPipeProducer;
}  // namespace mojo

namespace pdf {

// Writes out a response containing an <embed> with type
// "application/x-google-chrome-pdf". `PdfURLLoaderRequestInterceptor` replaces
// PDF loads with this response in order to trigger loading `source_url` with
// `chrome_pdf::PdfViewWebPlugin` instead.
class PluginResponseWriter final {
 public:
  PluginResponseWriter(
      const PdfStreamDelegate::StreamInfo& stream_info,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);
  PluginResponseWriter(const PluginResponseWriter&) = delete;
  PluginResponseWriter& operator=(const PluginResponseWriter&) = delete;
  ~PluginResponseWriter();

  // Starts sending the response, calling `done_callback` once the entire
  // response is sent to (but not necessarily received by) the
  // `URLLoaderClient`.
  //
  // Caller is responsible for keeping this response writer alive until
  // `done_callback` is called.
  void Start(base::OnceClosure done_callback);

 private:
  void OnWrite(base::OnceClosure done_callback, MojoResult result);

  std::string body_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  std::unique_ptr<mojo::DataPipeProducer> producer_;
  bool require_corp_ = false;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PLUGIN_RESPONSE_WRITER_H_
