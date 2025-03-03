// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/cleanup_task.h"

#import <CoreFoundation/CoreFoundation.h>

#include "base/apple/foundation_util.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/updater/updater_branding.h"

namespace updater {

void CleanOldCrxCache() {
  base::FilePath cache_app;
  if (!base::apple::GetLocalDirectory(NSCachesDirectory, &cache_app) ||
      !cache_app.IsAbsolute()) {
    return;
  }
  cache_app = cache_app.Append(MAC_BUNDLE_IDENTIFIER_STRING);
  // cache_app is now likely /Library/Caches/{MAC_BUNDLE_IDENTIFIER_STRING}

  base::stat_wrapper_t stat;
  base::File::Lstat(cache_app, &stat);
  if (stat.st_uid != geteuid()) {
    // The cache directory belongs to a different user; let them clean it.
    return;
  }
  if (stat.st_mode & 022) {
    // If other non-root users can write to the directory, avoid interacting
    // with it.
    return;
  }
  base::DeletePathRecursively(cache_app);
}

}  // namespace updater
