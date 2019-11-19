// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PATCH_CONTENT_PATCH_SERVICE_H_
#define COMPONENTS_SERVICES_PATCH_CONTENT_PATCH_SERVICE_H_

#include "base/callback.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace patch {

// Launches a new instance of the FilePatcher service in an isolated, sandboxed
// process, and returns a remote interface to control the service. The lifetime
// of the process is tied to that of the Remote. May be called from any thread.
mojo::PendingRemote<mojom::FilePatcher> LaunchFilePatcher();

}  // namespace patch

#endif  // COMPONENTS_SERVICES_PATCH_CONTENT_PATCH_SERVICE_H_
