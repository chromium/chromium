// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/content/unzip_service.h"

#include "base/no_destructor.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/service_process_host.h"

namespace unzip {

namespace {

LaunchCallback& GetLaunchOverride() {
  static base::NoDestructor<LaunchCallback> callback;
  return *callback;
}

}  // namespace

mojo::PendingRemote<mojom::Unzipper> LaunchUnzipper() {
  auto& launcher = GetLaunchOverride();
  if (launcher)
    return launcher.Run();

  mojo::PendingRemote<mojom::Unzipper> remote;
  content::ServiceProcessHost::Launch<mojom::Unzipper>(
      remote.InitWithNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithSandboxType(service_manager::SANDBOX_TYPE_UTILITY)
          .WithDisplayName(IDS_UNZIP_SERVICE_DISPLAY_NAME)
          .Pass());
  return remote;
}

void SetUnzipperLaunchOverrideForTesting(LaunchCallback callback) {
  GetLaunchOverride() = std::move(callback);
}

}  //  namespace unzip
