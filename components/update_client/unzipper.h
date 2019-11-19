// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UNZIPPER_H_
#define COMPONENTS_UPDATE_CLIENT_UNZIPPER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
}  // namespace base

namespace update_client {

class Unzipper {
 public:
  using UnzipCompleteCallback = base::OnceCallback<void(bool success)>;

  virtual ~Unzipper() = default;

  virtual void Unzip(const base::FilePath& zip_file,
                     const base::FilePath& destination,
                     UnzipCompleteCallback callback) = 0;

 protected:
  Unzipper() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Unzipper);
};

class UnzipperFactory : public base::RefCountedThreadSafe<UnzipperFactory> {
 public:
  virtual std::unique_ptr<Unzipper> Create() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<UnzipperFactory>;
  UnzipperFactory() = default;
  virtual ~UnzipperFactory() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnzipperFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UNZIPPER_H_
