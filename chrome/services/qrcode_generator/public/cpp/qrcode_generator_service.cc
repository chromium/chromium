// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "chrome/services/qrcode_generator/qrcode_generator_service_impl.h"
#include "content/public/browser/service_process_host.h"

namespace qrcode_generator {

namespace {

void MeasureDurationAndForwardToOriginalCallback(
    base::TimeTicks start_time,
    QRImageGenerator::ResponseCallback original_callback,
    mojom::GenerateQRCodeResponsePtr response) {
  base::UmaHistogramTimes("Sharing.QRCodeGeneration.Duration",
                          base::TimeTicks::Now() - start_time);

  std::move(original_callback).Run(std::move(response));
}

}  // namespace

QRImageGenerator::QRImageGenerator() = default;

QRImageGenerator::~QRImageGenerator() = default;

void QRImageGenerator::GenerateQRCode(mojom::GenerateQRCodeRequestPtr request,
                                      ResponseCallback callback) {
  ResponseCallback timed_callback =
      base::BindOnce(&MeasureDurationAndForwardToOriginalCallback,
                     base::TimeTicks::Now(), std::move(callback));

  // Using a WeakPtr below meets the following requirement from the doc comment:
  // "The `callback` will not be run if `this` generator is destroyed first".
  ResponseCallback weak_callback =
      base::BindOnce(&QRImageGenerator::ForwardResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(timed_callback));

  // Execute either a mojo call or an in-process C++ call depending on whether
  // the "RustyQrCodeGenerator" feature has been enabled.
  QRCodeGeneratorServiceImpl().GenerateQRCode(std::move(request),
                                              std::move(weak_callback));
}

void QRImageGenerator::ForwardResponse(
    ResponseCallback original_callback,
    mojom::GenerateQRCodeResponsePtr response) {
  std::move(original_callback).Run(std::move(response));
}

}  //  namespace qrcode_generator
