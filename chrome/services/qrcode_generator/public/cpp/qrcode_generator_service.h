// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_
#define CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace qrcode_generator {

class QRImageGenerator {
 public:
  // Launches a new instance of the `mojom::QRCodeGeneratorService` in an
  // isolated, sandboxed process. The lifetime of the process is tied to that of
  // the constructed `QRImageGenerator`.
  //
  // May be called from any thread. (Note that mojo requires that `Generate` is
  // called on the same thread.)
  QRImageGenerator();

  ~QRImageGenerator();

  QRImageGenerator(const QRImageGenerator&) = delete;
  QRImageGenerator(const QRImageGenerator&&) = delete;
  QRImageGenerator& operator=(const QRImageGenerator&) = delete;
  QRImageGenerator& operator=(const QRImageGenerator&&) = delete;

  // Generates a QR code.
  //
  // The `callback` will not be run if `this` generator is destroyed first.
  using ResponseCallback =
      base::OnceCallback<void(mojom::GenerateQRCodeResponsePtr)>;
  void GenerateQRCode(mojom::GenerateQRCodeRequestPtr request,
                      ResponseCallback callback);

 private:
  void ForwardResponse(ResponseCallback original_callback,
                       mojom::GenerateQRCodeResponsePtr response);

  mojo::Remote<mojom::QRCodeGeneratorService> mojo_service_;
  base::WeakPtrFactory<QRImageGenerator> weak_ptr_factory_;
};

}  // namespace qrcode_generator

#endif  // CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_
