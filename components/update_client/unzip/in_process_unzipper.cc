// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/unzip/in_process_unzipper.h"

#include <utility>

#include "base/files/file_path.h"
#include "third_party/zlib/google/zip.h"

namespace update_client {

namespace {

class InProcessUnzipper : public Unzipper {
 public:
  InProcessUnzipper() = default;

  void Unzip(const base::FilePath& zip_path,
             const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    std::move(callback).Run(zip::Unzip(zip_path, output_path));
  }
};

}  // namespace

InProcessUnzipperFactory::InProcessUnzipperFactory() = default;

std::unique_ptr<Unzipper> InProcessUnzipperFactory::Create() const {
  return std::make_unique<InProcessUnzipper>();
}

InProcessUnzipperFactory::~InProcessUnzipperFactory() = default;

}  // namespace update_client
