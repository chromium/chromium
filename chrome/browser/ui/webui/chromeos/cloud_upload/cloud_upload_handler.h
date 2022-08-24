// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_HANDLER_H_

#include "base/files/file_path.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace chromeos::cloud_upload {

base::FilePath GenerateDestinationPath(Profile* profile);

void UploadToCloud(Profile* profile, const storage::FileSystemURL& file_url);

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_HANDLER_H_
