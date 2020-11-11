// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser_factory.h"

#include "base/callback_helpers.h"
#include "components/web_package/web_bundle_parser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"

namespace web_package {

namespace {

class FileDataSource final : public mojom::BundleDataSource {
 public:
  FileDataSource(mojo::PendingReceiver<mojom::BundleDataSource> receiver,
                 base::File file)
      : receiver_(this, std::move(receiver)), file_(std::move(file)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &base::DeletePointer<FileDataSource>, base::Unretained(this)));
  }

 private:
  // Implements mojom::BundleDataSource.
  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    std::vector<uint8_t> buf(length);
    int bytes = file_.Read(offset, reinterpret_cast<char*>(buf.data()), length);
    if (bytes > 0) {
      buf.resize(bytes);
      std::move(callback).Run(std::move(buf));
    } else {
      std::move(callback).Run(base::nullopt);
    }
  }

  mojo::Receiver<mojom::BundleDataSource> receiver_;
  base::File file_;

  DISALLOW_COPY_AND_ASSIGN(FileDataSource);
};

}  // namespace

WebBundleParserFactory::WebBundleParserFactory() = default;

WebBundleParserFactory::~WebBundleParserFactory() = default;

std::unique_ptr<mojom::BundleDataSource>
WebBundleParserFactory::CreateFileDataSourceForTesting(
    mojo::PendingReceiver<mojom::BundleDataSource> receiver,
    base::File file) {
  return std::make_unique<FileDataSource>(std::move(receiver), std::move(file));
}

void WebBundleParserFactory::GetParserForFile(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    base::File file) {
  mojo::PendingRemote<mojom::BundleDataSource> remote_data_source;
  auto data_source = std::make_unique<FileDataSource>(
      remote_data_source.InitWithNewPipeAndPassReceiver(), std::move(file));
  GetParserForDataSource(std::move(receiver), std::move(remote_data_source));

  // |data_source| will be destructed on |remote_data_source| destruction.
  data_source.release();
}

void WebBundleParserFactory::GetParserForDataSource(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    mojo::PendingRemote<mojom::BundleDataSource> data_source) {
  auto parser = std::make_unique<WebBundleParser>(std::move(receiver),
                                                  std::move(data_source));

  // |parser| will be destructed on remote mojo ends' disconnection.
  parser.release();
}

}  // namespace web_package
