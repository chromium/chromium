// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UNZIP_IN_PROCESS_UNZIPPER_H_
#define COMPONENTS_UPDATE_CLIENT_UNZIP_IN_PROCESS_UNZIPPER_H_

#include <memory>

#include "components/update_client/unzipper.h"

namespace update_client {

// Creates an in process unzipper. It doesn't use Mojo abstractions and calls
// the unzip lib APIs directly. This should only be used for testing
// environments or other runtimes where multiprocess is infeasible, such as iOS,
// Android WebView or Content dependencies are not allowed.
class InProcessUnzipperFactory : public UnzipperFactory {
 public:
  InProcessUnzipperFactory();
  InProcessUnzipperFactory(const InProcessUnzipperFactory&) = delete;
  InProcessUnzipperFactory& operator=(const InProcessUnzipperFactory&) = delete;

  std::unique_ptr<Unzipper> Create() const override;

 protected:
  ~InProcessUnzipperFactory() override;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UNZIP_IN_PROCESS_UNZIPPER_H_
