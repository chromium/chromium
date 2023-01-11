// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UNZIPPER_H_
#define COMPONENTS_UPDATE_CLIENT_UNZIPPER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
}  // namespace base

namespace update_client {

class Unzipper {
 public:
  using UnzipCompleteCallback = base::OnceCallback<void(bool success)>;

  Unzipper(const Unzipper&) = delete;
  Unzipper& operator=(const Unzipper&) = delete;

  virtual ~Unzipper() = default;

  virtual void Unzip(const base::FilePath& zip_file,
                     const base::FilePath& destination,
                     UnzipCompleteCallback callback) = 0;

 protected:
  Unzipper() = default;
};

class UnzipperFactory : public base::RefCountedThreadSafe<UnzipperFactory> {
 public:
  UnzipperFactory(const UnzipperFactory&) = delete;
  UnzipperFactory& operator=(const UnzipperFactory&) = delete;

  virtual std::unique_ptr<Unzipper> Create() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<UnzipperFactory>;
  UnzipperFactory() = default;
  virtual ~UnzipperFactory() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UNZIPPER_H_
