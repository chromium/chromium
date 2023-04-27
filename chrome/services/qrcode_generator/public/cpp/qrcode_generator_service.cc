// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "content/public/browser/service_process_host.h"

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
    : mojo_service_(LaunchQRCodeGeneratorService()) {}

QRImageGenerator::~QRImageGenerator() = default;

void QRImageGenerator::GenerateQRCode(mojom::GenerateQRCodeRequestPtr request,
                                      ResponseCallback callback) {
  mojo_service_->GenerateQRCode(std::move(request), std::move(callback));
}

}  //  namespace qrcode_generator
