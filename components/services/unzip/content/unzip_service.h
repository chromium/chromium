// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_UNZIP_CONTENT_UNZIP_SERVICE_H_
#define COMPONENTS_SERVICES_UNZIP_CONTENT_UNZIP_SERVICE_H_

#include "base/callback.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace unzip {

// Launches a new instance of the Unzipper service in an isolated, sandboxed
// process, and returns a remote interface to control the service. The lifetime
// of the process is tied to that of the Remote. May be called from any thread.
mojo::PendingRemote<mojom::Unzipper> LaunchUnzipper();

// Overrides the logic used by |LaunchUnzipper()| to produce a remote Unzipper,
// allowing tests to set up an in-process instance to be used instead of an
// out-of-process instance.
using LaunchCallback =
    base::RepeatingCallback<mojo::PendingRemote<mojom::Unzipper>()>;
void SetUnzipperLaunchOverrideForTesting(LaunchCallback callback);

}  // namespace unzip

#endif  // COMPONENTS_SERVICES_UNZIP_CONTENT_UNZIP_SERVICE_H_
