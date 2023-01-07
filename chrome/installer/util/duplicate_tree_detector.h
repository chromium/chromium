// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a helper function that will check to see if a given folder
// is "identical" to another (for some value of identical, see below).
//

#ifndef CHROME_INSTALLER_UTIL_DUPLICATE_TREE_DETECTOR_H_
#define CHROME_INSTALLER_UTIL_DUPLICATE_TREE_DETECTOR_H_

namespace base {
class FilePath;
}

namespace installer {

// Returns true if |dest_path| contains all the files from |src_path| in the
// same directory structure and each of those files is of the same length.
// src_path_ and |dest_path| must either both be files or both be directories.
// Note that THIS IS A WEAK DEFINITION OF IDENTICAL and is intended only to
// catch cases of missing files or obvious modifications.
// It notably DOES NOT CHECKSUM the files.
bool IsIdenticalFileHierarchy(const base::FilePath& src_path,
                              const base::FilePath& dest_path);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_DUPLICATE_TREE_DETECTOR_H_
