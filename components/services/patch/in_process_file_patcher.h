// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PATCH_IN_PROCESS_FILE_PATCHER_H_
#define COMPONENTS_SERVICES_PATCH_IN_PROCESS_FILE_PATCHER_H_

#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace patch {

// Creates an in-process instance of the FilePatcher service on a background
// sequence and returns a PendingRemote which can be bound to communicate with
// the service. This should only be used for testing environments or other
// runtimes where multiprocess is infeasible, such as iOS, or Content
// dependencies are not allowed.
mojo::PendingRemote<mojom::FilePatcher> LaunchInProcessFilePatcher();

}  // namespace patch

#endif  // COMPONENTS_SERVICES_PATCH_IN_PROCESS_FILE_PATCHER_H_
