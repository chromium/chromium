// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace qrcode_generator {

namespace {

mojo::Remote<mojom::QRCodeGeneratorService> LaunchQRCodeGeneratorService() {
  return content::ServiceProcessHost::Launch<mojom::QRCodeGeneratorService>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_QRCODE_GENERATOR_SERVICE_NAME)
          .Pass());
}

}  // namespace

QRImageGenerator::QRImageGenerator()
    : mojo_service_(LaunchQRCodeGeneratorService()), weak_ptr_factory_(this) {}

QRImageGenerator::~QRImageGenerator() = default;

void QRImageGenerator::GenerateQRCode(mojom::GenerateQRCodeRequestPtr request,
                                      ResponseCallback callback) {
  // TODO(https://crbug.com/1431991): Wrap `callback` in a time-measuring,
  // UMA-reporting proxy.

  // Using a WeakPtr below meets the following requirement from the doc comment:
  // "The `callback` will not be run if `this` generator is destroyed first".
  ResponseCallback weak_callback =
      base::BindOnce(&QRImageGenerator::ForwardResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  // Call `callback` even after a mojo connection error.
  mojom::GenerateQRCodeResponsePtr connection_error_response =
      mojom::GenerateQRCodeResponse::New();
  connection_error_response->error_code =
      mojom::QRCodeGeneratorError::UNKNOWN_ERROR;
  ResponseCallback mojo_error_immune_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(weak_callback), std::move(connection_error_response));

  mojo_service_->GenerateQRCode(std::move(request),
                                std::move(mojo_error_immune_callback));
}

void QRImageGenerator::ForwardResponse(
    ResponseCallback original_callback,
    mojom::GenerateQRCodeResponsePtr response) {
  std::move(original_callback).Run(std::move(response));
}

}  //  namespace qrcode_generator
