// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "chrome/services/service_sandbox_type.h"
#include "content/public/browser/service_process_host.h"

namespace qrcode_generator {

mojo::Remote<mojom::QRCodeGeneratorService> LaunchQRCodeGeneratorService() {
  // TODO: check default sandboxtype
  return content::ServiceProcessHost::Launch<mojom::QRCodeGeneratorService>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_QRCODE_GENERATOR_SERVICE_NAME)
          .Pass());
}

}  //  namespace qrcode_generator
