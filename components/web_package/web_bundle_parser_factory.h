// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_FACTORY_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_FACTORY_H_

#include <memory>
#include <optional>

#include "base/files/file.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace web_package {

class WebBundleParserFactory : public mojom::WebBundleParserFactory {
 public:
  WebBundleParserFactory();

  WebBundleParserFactory(const WebBundleParserFactory&) = delete;
  WebBundleParserFactory& operator=(const WebBundleParserFactory&) = delete;

  ~WebBundleParserFactory() override;

  std::unique_ptr<mojom::BundleDataSource> CreateFileDataSourceForTesting(
      base::File file);

 private:
  // mojom::WebBundleParserFactory implementation.
  void GetParserForDataSource(
      mojo::PendingReceiver<mojom::WebBundleParser> receiver,
      const std::optional<GURL>& base_url,
      mojo::PendingRemote<mojom::BundleDataSource> data_source) override;

  void BindFileDataSource(mojo::PendingReceiver<mojom::BundleDataSource>
                              data_source_pending_receiver,
                          base::File file) override;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_FACTORY_H_
