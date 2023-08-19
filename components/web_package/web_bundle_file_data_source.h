// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_FILE_DATA_SOURCE_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_FILE_DATA_SOURCE_H_

#include <memory>

#include "base/files/file.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"

namespace web_package {

class WebBundleFileDataSource final
    : public mojo::DataPipeProducer::DataSource {
 public:
  static std::unique_ptr<WebBundleFileDataSource>
  CreateDataSource(base::File file, uint64_t offset, uint64_t length);

  WebBundleFileDataSource(const WebBundleFileDataSource&) = delete;
  WebBundleFileDataSource& operator=(const WebBundleFileDataSource&) = delete;

  ~WebBundleFileDataSource() override;

 private:
  WebBundleFileDataSource(base::File file, uint64_t offset, uint64_t length);
  // Implements `mojo::DataPipeProducer::DataSource`. The following methods
  // are called on a blockable sequenced task runner.
  uint64_t GetLength() const override;
  ReadResult Read(uint64_t offset, base::span<char> buffer) override;

  base::File file_;
  MojoResult error_;
  uint64_t offset_;
  uint64_t length_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_FILE_DATA_SOURCE_H_
