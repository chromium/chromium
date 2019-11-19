// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_QUARANTINE_CONNECTION_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_QUARANTINE_CONNECTION_H_

#include "base/callback.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace download {

// A callback which can be used to acquire a connection to a Quarantine
// Service instance if available.
using QuarantineConnectionCallback = base::RepeatingCallback<void(
    mojo::PendingReceiver<quarantine::mojom::Quarantine>)>;

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_QUARANTINE_CONNECTION_H_
