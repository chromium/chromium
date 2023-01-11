// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser_factory.h"

#include "base/functional/callback_helpers.h"
#include "components/web_package/web_bundle_parser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

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

  FileDataSource(const FileDataSource&) = delete;
  FileDataSource& operator=(const FileDataSource&) = delete;

 private:
  // Implements mojom::BundleDataSource.
  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    std::vector<uint8_t> buf(length);
    int bytes = file_.Read(offset, reinterpret_cast<char*>(buf.data()), length);
    if (bytes > 0) {
      buf.resize(bytes);
      std::move(callback).Run(std::move(buf));
    } else {
      std::move(callback).Run(absl::nullopt);
    }
  }

  void Length(LengthCallback callback) override {
    const int64_t length = file_.GetLength();
    std::move(callback).Run(length);
  }

  void IsRandomAccessContext(IsRandomAccessContextCallback callback) override {
    std::move(callback).Run(true);
  }

  mojo::Receiver<mojom::BundleDataSource> receiver_;
  base::File file_;
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
    const absl::optional<GURL>& base_url,
    base::File file) {
  mojo::PendingRemote<mojom::BundleDataSource> remote_data_source;
  auto data_source = std::make_unique<FileDataSource>(
      remote_data_source.InitWithNewPipeAndPassReceiver(), std::move(file));
  GetParserForDataSource(std::move(receiver), base_url,
                         std::move(remote_data_source));

  // |data_source| will be destructed on |remote_data_source| destruction.
  data_source.release();
}

void WebBundleParserFactory::GetParserForDataSource(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    const absl::optional<GURL>& base_url,
    mojo::PendingRemote<mojom::BundleDataSource> data_source) {
  // TODO(crbug.com/1247939): WebBundleParserFactory doesn't support |base_url|.
  // For features::kWebBundlesFromNetwork should support |base_url|.
  auto parser = std::make_unique<WebBundleParser>(
      std::move(receiver), std::move(data_source), base_url.value_or(GURL()));

  // |parser| will be destructed on remote mojo ends' disconnection.
  parser.release();
}

}  // namespace web_package
