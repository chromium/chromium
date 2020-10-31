// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/fake_holding_space_color_provider.h"

#include "base/files/file_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"

namespace ash {
namespace holding_space {

SkColor FakeHoldingSpaceColorProvider::GetBackgroundColor() const {
  return SkColor();
}
SkColor FakeHoldingSpaceColorProvider::GetFileIconColor() const {
  return SkColor();
}

}  // namespace holding_space
}  // namespace ash