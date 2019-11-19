// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_UNZIP_IN_PROCESS_UNZIPPER_H_
#define COMPONENTS_SERVICES_UNZIP_IN_PROCESS_UNZIPPER_H_

#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace unzip {

// Creates an in-process instance of the Unzipper service on a background
// sequence and returns a PendingRemote which can be bound to communicate with
// the service. This should only be used for testing environments or other
// runtimes where multiprocess is infeasible, such as iOS, or Content
// dependencies are not allowed.
mojo::PendingRemote<mojom::Unzipper> LaunchInProcessUnzipper();

}  // namespace unzip

#endif  // COMPONENTS_SERVICES_UNZIP_IN_PROCESS_UNZIPPER_H_
