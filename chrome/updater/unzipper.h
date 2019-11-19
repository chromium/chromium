// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UNZIPPER_H_
#define CHROME_UPDATER_UNZIPPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/unzipper.h"

namespace updater {

class UnzipperFactory : public update_client::UnzipperFactory {
 public:
  UnzipperFactory();

  std::unique_ptr<update_client::Unzipper> Create() const override;

 protected:
  ~UnzipperFactory() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnzipperFactory);
};

}  // namespace updater

#endif  // CHROME_UPDATER_UNZIPPER_H_
