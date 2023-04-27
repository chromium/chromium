// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_
#define CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_

#include "base/functional/callback.h"
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
  //
  // The `callback` may not run if there is an internal mojo connection error.
  // TODO(https://crbug.com/1431991): The public C++ interface should be
  // mojo-agnostic.  This part of the comment should be removed or replaced
  // after handling mojo connection errors internally.
  using ResponseCallback =
      base::OnceCallback<void(mojom::GenerateQRCodeResponsePtr)>;
  void GenerateQRCode(mojom::GenerateQRCodeRequestPtr request,
                      ResponseCallback callback);

  // TODO(https://crbug.com/1431991): The public C++ interface should be
  // mojo-agnostic.  Remove this method and handle mojo connection errors
  // internally.
  void set_disconnect_handler(base::OnceClosure callback) {
    mojo_service_.set_disconnect_handler(std::move(callback));
  }

 private:
  mojo::Remote<mojom::QRCodeGeneratorService> mojo_service_;
};

}  // namespace qrcode_generator

#endif  // CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_
