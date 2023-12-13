// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PATCHER_H_
#define COMPONENTS_UPDATE_CLIENT_PATCHER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace base {
class File;
}  // namespace base

namespace update_client {

class Patcher : public base::RefCountedThreadSafe<Patcher> {
 public:
  using PatchCompleteCallback = base::OnceCallback<void(int result)>;

  Patcher(const Patcher&) = delete;
  Patcher& operator=(const Patcher&) = delete;

  virtual void PatchPuffPatch(base::File input_file_path,
                              base::File patch_file_path,
                              base::File output_file_path,
                              PatchCompleteCallback callback) const = 0;

 protected:
  friend class base::RefCountedThreadSafe<Patcher>;
  Patcher() = default;
  virtual ~Patcher() = default;
};

class PatcherFactory : public base::RefCountedThreadSafe<PatcherFactory> {
 public:
  PatcherFactory(const PatcherFactory&) = delete;
  PatcherFactory& operator=(const PatcherFactory&) = delete;

  virtual scoped_refptr<Patcher> Create() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<PatcherFactory>;
  PatcherFactory() = default;
  virtual ~PatcherFactory() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PATCHER_H_
