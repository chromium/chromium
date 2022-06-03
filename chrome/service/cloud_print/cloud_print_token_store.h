// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_TOKEN_STORE_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_TOKEN_STORE_H_

#include <string>
#include "base/check_op.h"
#include "base/threading/thread_checker.h"

// This class serves as the single repository for cloud print auth tokens. This
// is only used within the CloudPrintProxyCoreThread.

namespace cloud_print {

class CloudPrintTokenStore {
 public:
  // Returns the CloudPrintTokenStore instance for this thread. Will be NULL
  // if no instance was created in this thread before.
  static CloudPrintTokenStore* current();

  CloudPrintTokenStore();

  CloudPrintTokenStore(const CloudPrintTokenStore&) = delete;
  CloudPrintTokenStore& operator=(const CloudPrintTokenStore&) = delete;

  ~CloudPrintTokenStore();

  void SetToken(const std::string& token);
  std::string token() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return token_;
  }

 private:
  std::string token_;

  // Thread-affine per use of TLS in impl.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_TOKEN_STORE_H_
