// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/ipp_parser/ipp_parser_service.h"

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace ipp_parser {

mojo::PendingRemote<mojom::IppParser> LaunchIppParser() {
  mojo::PendingRemote<mojom::IppParser> remote;
  content::ServiceProcessHost::Launch<mojom::IppParser>(
      remote.InitWithNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithSandboxType(service_manager::SANDBOX_TYPE_UTILITY)
          .WithDisplayName(IDS_UTILITY_PROCESS_IPP_PARSER_SERVICE_NAME)
          .Pass());
  return remote;
}

}  //  namespace ipp_parser
