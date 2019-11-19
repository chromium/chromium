// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/patch/content/patch_service.h"

#include "base/no_destructor.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/service_process_host.h"

namespace patch {

mojo::PendingRemote<mojom::FilePatcher> LaunchFilePatcher() {
  mojo::PendingRemote<mojom::FilePatcher> remote;
  content::ServiceProcessHost::Launch<mojom::FilePatcher>(
      remote.InitWithNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithSandboxType(service_manager::SANDBOX_TYPE_UTILITY)
          .WithDisplayName(IDS_PATCH_SERVICE_DISPLAY_NAME)
          .Pass());
  return remote;
}

}  //  namespace patch
