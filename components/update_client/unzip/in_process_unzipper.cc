// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/unzip/in_process_unzipper.h"

#include <utility>

#include "base/files/file_path.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "third_party/zlib/google/zip.h"

namespace update_client {

namespace {

class InProcessUnzipper : public Unzipper {
 public:
  explicit InProcessUnzipper(
      InProcessUnzipperFactory::SymlinkOption symlink_option)
      : symlink_option_(symlink_option) {}

  void Unzip(const base::FilePath& zip_path,
             const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    std::move(callback).Run(
        zip::Unzip(zip_path, output_path, /*options=*/{}, symlink_option_));
  }

  base::OnceClosure DecodeXz(const base::FilePath& xz_file,
                             const base::FilePath& destination,
                             UnzipCompleteCallback callback) override {
    return unzip::DecodeXz(unzip::LaunchInProcessUnzipper(), xz_file,
                           destination, std::move(callback));
  }

 private:
  const InProcessUnzipperFactory::SymlinkOption symlink_option_;
};

}  // namespace

InProcessUnzipperFactory::InProcessUnzipperFactory(SymlinkOption symlink_option)
    : symlink_option_(symlink_option) {}

std::unique_ptr<Unzipper> InProcessUnzipperFactory::Create() const {
  return std::make_unique<InProcessUnzipper>(symlink_option_);
}

InProcessUnzipperFactory::~InProcessUnzipperFactory() = default;

}  // namespace update_client
