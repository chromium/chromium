// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser_factory.h"

#include <optional>

#include "base/compiler_specific.h"
#include "base/functional/callback_helpers.h"
#include "components/web_package/web_bundle_parser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

namespace web_package {

namespace {

class FileDataSource final : public mojom::BundleDataSource {
 public:
  explicit FileDataSource(base::File file) : file_(std::move(file)) {}

  FileDataSource(const FileDataSource&) = delete;
  FileDataSource& operator=(const FileDataSource&) = delete;

 private:
  // Implements mojom::BundleDataSource.
  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    std::vector<uint8_t> buf(length);
    int bytes = UNSAFE_TODO(
        file_.Read(offset, reinterpret_cast<char*>(buf.data()), length));
    if (bytes > 0) {
      buf.resize(bytes);
      std::move(callback).Run(std::move(buf));
    } else {
      std::move(callback).Run(std::nullopt);
    }
  }

  void Length(LengthCallback callback) override {
    const int64_t length = file_.GetLength();
    std::move(callback).Run(length);
  }

  void IsRandomAccessContext(IsRandomAccessContextCallback callback) override {
    std::move(callback).Run(true);
  }

  void Close(CloseCallback callback) override {
    file_.Close();
    std::move(callback).Run();
  }

  base::File file_;
};

}  // namespace

WebBundleParserFactory::WebBundleParserFactory() = default;

WebBundleParserFactory::~WebBundleParserFactory() = default;

std::unique_ptr<mojom::BundleDataSource>
WebBundleParserFactory::CreateFileDataSourceForTesting(base::File file) {
  return std::make_unique<FileDataSource>(std::move(file));
}

void WebBundleParserFactory::GetParserForDataSource(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    const std::optional<GURL>& base_url,
    mojo::PendingRemote<mojom::BundleDataSource> data_source) {
  // TODO(crbug.com/40197063): WebBundleParserFactory doesn't support
  // |base_url|. For features::kWebBundlesFromNetwork should support |base_url|.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebBundleParser>(std::move(data_source),
                                        base_url.value_or(GURL())),
      std::move(receiver));
}

void WebBundleParserFactory::BindFileDataSource(
    mojo::PendingReceiver<mojom::BundleDataSource> data_source_pending_receiver,
    base::File file) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FileDataSource>(std::move(file)),
                              std::move(data_source_pending_receiver));
}

}  // namespace web_package
