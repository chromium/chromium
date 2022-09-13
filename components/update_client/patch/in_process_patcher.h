// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PATCH_IN_PROCESS_PATCHER_H_
#define COMPONENTS_UPDATE_CLIENT_PATCH_IN_PROCESS_PATCHER_H_

#include "base/memory/scoped_refptr.h"
#include "components/update_client/patcher.h"

namespace update_client {

// Creates an in-process patcher. It doesn't use Mojo abstractions and calls
// Courgette lib APIs directly. This should only be used for testing
// environments or other runtimes where multiprocess is infeasible, such as iOS,
// Android WebView or Content dependencies are not allowed.
class InProcessPatcherFactory : public PatcherFactory {
 public:
  InProcessPatcherFactory();
  InProcessPatcherFactory(const InProcessPatcherFactory&) = delete;
  InProcessPatcherFactory& operator=(const InProcessPatcherFactory&) = delete;

  scoped_refptr<Patcher> Create() const override;

 protected:
  ~InProcessPatcherFactory() override;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PATCH_IN_PROCESS_PATCHER_H_
