// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_
#define CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_

#include "base/functional/callback.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"

namespace qrcode_generator {

class QRImageGenerator {
 public:
  QRImageGenerator();
  ~QRImageGenerator();

  QRImageGenerator(const QRImageGenerator&) = delete;
  QRImageGenerator(const QRImageGenerator&&) = delete;
  QRImageGenerator& operator=(const QRImageGenerator&) = delete;
  QRImageGenerator& operator=(const QRImageGenerator&&) = delete;

  // Generates a QR code.  `callback` will be invoked synchronously.
  using ResponseCallback =
      base::OnceCallback<void(mojom::GenerateQRCodeResponsePtr)>;
  void GenerateQRCode(mojom::GenerateQRCodeRequestPtr request,
                      ResponseCallback callback);
};

}  // namespace qrcode_generator

#endif  // CHROME_SERVICES_QRCODE_GENERATOR_PUBLIC_CPP_QRCODE_GENERATOR_SERVICE_H_
