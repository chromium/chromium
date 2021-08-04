// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/plugin_response_writer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace pdf {

namespace {

// The background color for the PDF viewer in string format. Keep it in-sync
// with `BACKGROUND_COLOR` in chrome/browser/resources/pdf/pdf_viewer.js.
static const char kPdfViewerBackgroundColor[] = "4283586137";

// TODO(https://crbug.com/1227206): Change the "background-color" attribute
// value depending on the actual plugin params. For example, when the plugin is
// for Print Preview, its background color should be different.
std::string GenerateResponse(const GURL& source_url, const GURL& original_url) {
  // TODO(crbug.com/1228987): This script in this response is never executed
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
</style>
<embed type="application/x-google-chrome-pdf" src="$1" original-url="$2"
       background-color="$3">
<script>
const channel = new MessageChannel();
const plugin = document.querySelector('embed');

plugin.addEventListener('message', e => channel.port1.postMessage(e.data));
channel.port1.onmessage = e => plugin.postMessage(e.data);

window.parent.postMessage(
    {type: 'connect', token: plugin.getAttribute('src')}, '$4',
    [channel.port2]);
</script>
)";

  return base::ReplaceStringPlaceholders(
      kResponseTemplate,
      {source_url.spec(), original_url.spec(), kPdfViewerBackgroundColor,
       source_url.GetOrigin().spec()},
      /*offsets=*/nullptr);
}

}  // namespace

PluginResponseWriter::PluginResponseWriter(
    const GURL& source_url,
    const GURL& original_url,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
    : body_(GenerateResponse(source_url, original_url)),
      client_(std::move(client)) {}

PluginResponseWriter::~PluginResponseWriter() = default;

void PluginResponseWriter::Start(base::OnceClosure done_callback) {
  auto response = network::mojom::URLResponseHead::New();
  response->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response->mime_type = "text/html";
  client_->OnReceiveResponse(std::move(response));

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (mojo::CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
    client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    std::move(done_callback).Run();
    return;
  }

  client_->OnStartLoadingResponseBody(std::move(consumer));

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
