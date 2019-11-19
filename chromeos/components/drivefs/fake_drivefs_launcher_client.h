// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_LAUNCHER_CLIENT_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_LAUNCHER_CLIENT_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "chromeos/components/drivefs/mojom/fake_drivefs_launcher.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace drivefs {

class FakeDriveFsLauncherClient {
 public:
  COMPONENT_EXPORT(DRIVEFS)
  static void Init(const base::FilePath& socket_path,
                   const base::FilePath& chroot_path);

 private:
  friend class base::NoDestructor<FakeDriveFsLauncherClient>;

  FakeDriveFsLauncherClient(const base::FilePath& chroot_path,
                            const base::FilePath& socket_path);
  ~FakeDriveFsLauncherClient();

  base::FilePath MaybeMountDriveFs(
      const std::string& source_path,
      const std::vector<std::string>& mount_options);

  const base::FilePath chroot_path_;
  const base::FilePath socket_path_;

  mojo::Remote<mojom::FakeDriveFsLauncher> launcher_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveFsLauncherClient);
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_LAUNCHER_CLIENT_H_
