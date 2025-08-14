// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_OUT_OF_PROCESS_UNZIPPER_H_
#define CHROME_UPDATER_OUT_OF_PROCESS_UNZIPPER_H_

#include "base/memory/scoped_refptr.h"
#include "components/update_client/unzipper.h"

namespace updater {

class OutOfProcessUnzipper : public update_client::Unzipper {
 public:
  OutOfProcessUnzipper();

  OutOfProcessUnzipper(const OutOfProcessUnzipper&) = delete;
  OutOfProcessUnzipper& operator=(const OutOfProcessUnzipper&) = delete;

  ~OutOfProcessUnzipper() override;

  // Overrides for update_client::Unzipper.
  void Unzip(const base::FilePath& zip_path,
             const base::FilePath& output_path,
             UnzipCompleteCallback done_callback) override;
  base::OnceClosure DecodeXz(const base::FilePath& xz_path,
                             const base::FilePath& output_path,
                             UnzipCompleteCallback done_callback) override;
};

// Creates an out-of-process unzipper.
class OutOfProcessUnzipperFactory : public update_client::UnzipperFactory {
 public:
  OutOfProcessUnzipperFactory();

  // Overrides for update_client::UnzipperFactory.
  std::unique_ptr<update_client::Unzipper> Create() const override;

 protected:
  ~OutOfProcessUnzipperFactory() override = default;
};

}  // namespace updater

#endif  // CHROME_UPDATER_OUT_OF_PROCESS_UNZIPPER_H_
