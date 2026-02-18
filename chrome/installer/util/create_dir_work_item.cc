// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/create_dir_work_item.h"

#include <windows.h>

#include <ostream>

#include "base/check.h"
#include "base/logging.h"

namespace {

// Creates the directory `this_path`; recursing toward the root of the volume to
// create its parent directories as far as necessary. `top_path` is set to the
// highest ancestor created. Returns:
// - ERROR_SUCCESS: `this_path` was created successfully.
// - ERROR_NO_MORE_FILES: `this_path` is the root of the volume.
// - ERROR_ALREADY_EXISTS: `this_path` exists as a file or a directory.
// - any other value: the reason that `this_path` could not be created.
DWORD Recurse(const base::FilePath& desired_path,
              const base::FilePath& this_path,
              base::FilePath& top_path) {
  if (this_path.DirName() == this_path) {  // Reached the root of the volume.
    LOG(ERROR) << "Failed to create any ancestor of directory " << desired_path;
    return ERROR_NO_MORE_FILES;
  }

  if (::CreateDirectory(this_path.value().c_str(),
                        /*lpSecurityAttributes=*/nullptr) != 0) {
    VLOG(1) << "Created directory " << this_path;
    // Remember that this was the first directory created.
    top_path = this_path;
    return ERROR_SUCCESS;
  }

  // If this is the deepest level of recursion, this error is reported to the
  // caller.
  auto error = ::GetLastError();
  if (error == ERROR_ALREADY_EXISTS) {
    VLOG(1) << "File or directory " << this_path << " already exists.";
    // There is no use in progressing closer to the root of the volume. The
    // level of recursion immediately prior to this one failed to create a
    // directory in `this_path`, so there is no use in trying to create it or
    // any other directories leading up to `desired_path` again.
    return ERROR_ALREADY_EXISTS;
  }

  // `this_path` either does not exist or could not be created. Advance one
  // level closer to the root.
  const auto next_error = Recurse(desired_path, this_path.DirName(), top_path);
  if (next_error != ERROR_SUCCESS) {
    // No directories were created by that attempt, so there is no use in trying
    // to create `this_path` again.
    if (next_error == ERROR_ALREADY_EXISTS) {
      // The parent exists. Log and report the failure to create `this_path`.
      ::SetLastError(error);
      PLOG(ERROR) << "Failed to create directory " << this_path.BaseName()
                  << " in existing parent " << this_path.DirName();
      return error;  // Return the error from trying to create `this_path`.
    }
    if (next_error == ERROR_NO_MORE_FILES) {
      // Reached the volume root. A suitable message has already been logged.
      return error;  // Return the error from trying to create `this_path`.
    }
    return next_error;  // Return the error from the ancestor.
  }

  // Try to create `this_path` again now that its parent has been created.
  if (::CreateDirectory(this_path.value().c_str(),
                        /*lpSecurityAttributes=*/nullptr) != 0) {
    VLOG(1) << "Created directory " << this_path;
    return ERROR_SUCCESS;
  }

  // A parent of `this_path` was created, but `this_path` itself wasn't.
  error = ::GetLastError();
  PLOG(ERROR) << "Failed to create directory " << this_path;
  return error;
}

}  // namespace

CreateDirWorkItem::~CreateDirWorkItem() = default;

CreateDirWorkItem::CreateDirWorkItem(const base::FilePath& path)
    : path_(path) {}

bool CreateDirWorkItem::DoImpl() {
  if (!path_.IsAbsolute()) {
    return false;
  }

  DWORD error = Recurse(path_, path_, top_path_);
  if (error == ERROR_SUCCESS) {
    // `path_` was created. `top_path_` is now its highest ancestor that was
    // created (possibly `path_` itself). A suitable message has already been
    // logged.
    return true;
  }

  // `Recurse` may have failed because a file or directory named `path_` already
  // exists. Report success if `path_` exists and is not a file.
  const DWORD attributes = ::GetFileAttributes(path_.value().c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    DWORD attribute_error = ::GetLastError();
    if (attribute_error == ERROR_FILE_NOT_FOUND ||
        attribute_error == ERROR_PATH_NOT_FOUND) {
      // `path_` was not created. A suitable error has already been logged.
      return false;
    }
    // Otherwise, the attributes could not be read for some reason. This could
    // be a result of a permissions issue (access denied or sharing violation,
    // for example). In this case, `path_` does exist. Presume that it's a
    // directory rather than a file. A subsequent attempt to use the directory
    // will fail with a suitable error message if it turns out to be a file or
    // inaccessible.
    PLOG(ERROR) << "Presuming that " << path_
                << " is a directory since its attributes cannot be read";
    return true;
  }
  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    // `path_` is a directory. A suitable message has already been logged.
    return true;
  }
  LOG(ERROR) << path_
             << " exists but is not a directory (attributes = " << attributes
             << ")";
  return false;
}

void CreateDirWorkItem::RollbackImpl() {
  if (top_path_.empty()) {
    return;  // No directories were created in the making of this film.
  }

  // Delete all empty directories from `path_` down to `top_path_`, inclusively.
  // Non-empty directories are intentionally left behind.
  base::FilePath path_to_delete(path_);

  while (true) {
    // RemoveDirectory is harmless (ERROR_DIR_NOT_EMPTY) if the directory is not
    // empty or if it doesn't exist (ERROR_FILE_NOT_FOUND). Attempt to delete
    // all directories that were created without regard to success or failure.
    ::RemoveDirectory(path_to_delete.value().c_str());
    if (path_to_delete == top_path_) {
      return;  // Just deleted the topmost directory created.
    }
    path_to_delete = path_to_delete.DirName();
  }
}
