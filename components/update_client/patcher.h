// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PATCHER_H_
#define COMPONENTS_UPDATE_CLIENT_PATCHER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
}  // namespace base

namespace update_client {

class Patcher : public base::RefCountedThreadSafe<Patcher> {
 public:
  using PatchCompleteCallback = base::OnceCallback<void(int result)>;

  virtual void PatchBsdiff(const base::FilePath& input_file,
                           const base::FilePath& patch_file,
                           const base::FilePath& destination,
                           PatchCompleteCallback callback) const = 0;

  virtual void PatchCourgette(const base::FilePath& input_file,
                              const base::FilePath& patch_file,
                              const base::FilePath& destination,
                              PatchCompleteCallback callback) const = 0;

 protected:
  friend class base::RefCountedThreadSafe<Patcher>;
  Patcher() = default;
  virtual ~Patcher() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Patcher);
};

class PatcherFactory : public base::RefCountedThreadSafe<PatcherFactory> {
 public:
  virtual scoped_refptr<Patcher> Create() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<PatcherFactory>;
  PatcherFactory() = default;
  virtual ~PatcherFactory() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PatcherFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PATCHER_H_
