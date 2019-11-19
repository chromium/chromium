// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_H_
#define CHROME_UPDATER_UTIL_H_

namespace base {
class FilePath;
}

namespace updater {

// Returns a directory where updater files or its data is stored.
bool GetProductDirectory(base::FilePath* path);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_H_
