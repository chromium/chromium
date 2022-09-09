// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/ipp_parser/public/cpp/browser/ipp_parser_launcher.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace ipp_parser {

mojo::PendingRemote<mojom::IppParser> LaunchIppParser() {
  mojo::PendingRemote<mojom::IppParser> remote;
  content::ServiceProcessHost::Launch<mojom::IppParser>(
      remote.InitWithNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_IPP_PARSER_SERVICE_NAME)
          .Pass());
  return remote;
}

}  //  namespace ipp_parser
