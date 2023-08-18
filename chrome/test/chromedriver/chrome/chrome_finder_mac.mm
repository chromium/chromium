// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"

void GetApplicationDirs(std::vector<base::FilePath>* app_dirs) {
  base::FilePath user_app_dir;
  if (base::apple::GetUserDirectory(NSApplicationDirectory, &user_app_dir)) {
    app_dirs->push_back(user_app_dir);
  }
  base::FilePath local_app_dir;
  if (base::apple::GetLocalDirectory(NSApplicationDirectory, &local_app_dir)) {
    app_dirs->push_back(local_app_dir);
  }
}
