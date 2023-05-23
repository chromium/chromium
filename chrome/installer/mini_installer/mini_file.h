// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_MINI_FILE_H_
#define CHROME_INSTALLER_MINI_INSTALLER_MINI_FILE_H_

#include <windows.h>

#include "chrome/installer/mini_installer/path_string.h"

namespace mini_installer {

// A simple abstraction over a path to a file and a Windows file handle to it.
class MiniFile {
 public:
  MiniFile();

  // Closes the file if the instance holds a valid handle. The file will be
  // deleted if directed by a call to DeleteOnClose().
  ~MiniFile();

  MiniFile(const MiniFile&) = delete;
  MiniFile(MiniFile&&) = delete;
  MiniFile& operator=(const MiniFile&) = delete;

  // Postcondition: other.path() will return an empty string and other.IsValid()
  // will return false.
  MiniFile& operator=(MiniFile&& other) noexcept;

  // Creates a new file at |path| for exclusive writing. Returns true if the
  // file was created, in which case IsValid() will return true. Consumers are
  // expected to write sequentially to the file. This expectation is for the
  // sake of performance rather than correctness.
  bool Create(const wchar_t* path);

  // Returns true if this object has a path and a handle to an open file.
  bool IsValid() const;

  // Marks the file for deletion when the handle is closed via Close() or the
  // instance's destructor. This state follows the handle when moved.
  bool DeleteOnClose();

  // Closes the handle and clears the path. Following this, IsValid() will
  // return false.
  void Close();

  // Returns a new handle to the file, or INVALID_HANDLE_VALUE on error.
  HANDLE DuplicateHandle() const;

  // Returns the path to the open file, or a pointer to an empty string if
  // IsValid() is false.
  const wchar_t* path() const { return path_.get(); }

  // Returns the open file handle. The caller must not close it, and must not
  // refer to it after this instance is closed or destroyed.
  HANDLE GetHandleUnsafe() const;

 private:
  // The path by which |handle_| was created or opened, or an empty path if
  // |handle_| is not valid.
  PathString path_;

  // A handle to the open file, or INVALID_HANDLE_VALUE.
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_MINI_FILE_H_
