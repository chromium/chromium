// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_
#define CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_

#include "base/callback.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace qrcode_generator {

// Launches a new instance of the QRCodeGeneratorService in an isolated,
// sandboxed process, and returns a remote interface to control the service. The
// lifetime of the process is tied to that of the Remote. May be called from any
// thread.
mojo::Remote<mojom::QRCodeGeneratorService> LaunchQRCodeGeneratorService();

}  // namespace qrcode_generator

#endif  // CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_
