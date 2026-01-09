// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_CAMERA_UPLOAD_DONE_NOTIFICATION_H_
#define CHROMEOS_ASH_EXPERIENCES_CAMERA_UPLOAD_DONE_NOTIFICATION_H_

#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

namespace gfx {
class Image;
}

void CreateUploadDoneNotification(bool onedrive,
                                  const gfx::Image& thumbnail,
                                  const base::FilePath& file_path,
                                  base::RepeatingClosure edit_callback,
                                  base::RepeatingClosure delete_callback);

#endif  // CHROMEOS_ASH_EXPERIENCES_CAMERA_UPLOAD_DONE_NOTIFICATION_H_
