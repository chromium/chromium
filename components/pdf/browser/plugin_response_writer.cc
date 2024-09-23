// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/plugin_response_writer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "pdf/pdf_features.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace pdf {

namespace {

// Generates a response ready to be used for creating the PDF loader. The
// returned value is a raw string in which the escape characters are not
// processed.
// Note: This function is security sensitive since it defines the boundary of
// HTML and the embedded PDF. Must limit information shared with the PDF plugin
// process through this response.
std::string GenerateResponse(const PdfStreamDelegate::StreamInfo& stream_info) {
  // TODO(crbug.com/40189769): This script in this response is never executed
  // when JavaScript is blocked throughout the browser (set in
  // chrome://settings/content/javascript). A permanent solution would likely
  // have to hook into postMessage() natively.
  static constexpr char kResponseTemplate[] = R"(<!DOCTYPE html>
<style>
body,
embed,
html {
  height: 100%;
  margin: 0;
  width: 100%;
}

embed {
  left: 0;
  position: fixed;
  top: 0;
}

/* Hide scrollbars when in Presentation mode. */
.fullscreen {
  overflow: hidden;
}
</style>
<div id="sizer"></div>
<embed type="application/x-google-chrome-pdf" src="$1" original-url="$2"
    background-color="$4" javascript="$5"$6$7>
<script type="module">
$3
</script>
)";

  // TODO(crbug.com/40792950): We should load the injected scripts as network
  // resources instead. Until then, feel free to raise this limit as necessary.
  if (stream_info.injected_script)
    DCHECK_LE(stream_info.injected_script->size(), 16'384u);

  return base::ReplaceStringPlaceholders(
      kResponseTemplate,
      {stream_info.stream_url.spec(), stream_info.original_url.spec(),
       stream_info.injected_script ? *stream_info.injected_script : "",
       base::NumberToString(stream_info.background_color),
       stream_info.allow_javascript ? "allow" : "block",
       stream_info.full_frame ? " full-frame" : "",
       stream_info.use_skia ? " use-skia" : ""},
      /*offsets=*/nullptr);
}

}  // namespace

PluginResponseWriter::PluginResponseWriter(
    const PdfStreamDelegate::StreamInfo& stream_info,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
    : body_(GenerateResponse(stream_info)),
      client_(std::move(client)),
      require_corp_(stream_info.require_corp) {}

PluginResponseWriter::~PluginResponseWriter() = default;

void PluginResponseWriter::Start(base::OnceClosure done_callback) {
  auto response = network::mojom::URLResponseHead::New();
  response->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  // Allow the PDF plugin to be embedded in cross-origin sites if the original
  // PDF has a COEP: require-corp header.
  if (chrome_pdf::features::IsOopifPdfEnabled() && require_corp_) {
    response->headers->AddHeader("Cross-Origin-Embedder-Policy",
                                 "require-corp");
    response->headers->AddHeader("Cross-Origin-Resource-Policy",
                                 "cross-origin");
  }
  response->mime_type = "text/html";

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (mojo::CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
    client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    std::move(done_callback).Run();
    return;
  }

  client_->OnReceiveResponse(std::move(response), std::move(consumer),
                             std::nullopt);

  producer_ = std::make_unique<mojo::DataPipeProducer>(std::move(producer));

  // Caller is required to keep `this` alive until `done_callback` is called, so
  // `base::Unretained(this)` should be safe.
  producer_->Write(
      std::make_unique<mojo::StringDataSource>(
          body_, mojo::StringDataSource::AsyncWritingMode::
                     STRING_STAYS_VALID_UNTIL_COMPLETION),
      base::BindOnce(&PluginResponseWriter::OnWrite, base::Unretained(this),
                     std::move(done_callback)));
}

void PluginResponseWriter::OnWrite(base::OnceClosure done_callback,
                                   MojoResult result) {
  producer_.reset();

  if (result == MOJO_RESULT_OK) {
    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = body_.size();
    status.encoded_body_length = body_.size();
    status.decoded_body_length = body_.size();
    client_->OnComplete(status);
  } else {
    client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
  }

  std::move(done_callback).Run();
}

}  // namespace pdf
