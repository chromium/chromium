// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_COPY_H_
#define COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_COPY_H_

#include <type_traits>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_helpers.h"
#include "components/file_access/scoped_file_access.h"

namespace file_access {

// Extension to the move-only class ScopedFileAccess. Beside the access token to
// the file this class also holds a scoped callback, which is called with the
// destruction of this object. That is used copy the source url information
// after a copy operation has finished.
class COMPONENT_EXPORT(FILE_ACCESS) ScopedFileAccessCopy
    : public ScopedFileAccess {
 public:
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  ScopedFileAccessCopy(bool allowed,
                       base::ScopedFD fd,
                       base::OnceClosure copy_end_callback);
#else
  ScopedFileAccessCopy(bool allowed, base::OnceClosure copy_end_callback);
#endif

  ScopedFileAccessCopy(ScopedFileAccessCopy&& other);
  ScopedFileAccessCopy& operator=(ScopedFileAccessCopy&& other);
  ScopedFileAccessCopy(const ScopedFileAccessCopy&) = delete;
  ScopedFileAccessCopy& operator=(const ScopedFileAccessCopy&) = delete;
  ~ScopedFileAccessCopy() override;

 private:
  base::ScopedClosureRunner copy_end_callback_;
};

}  // namespace file_access

#endif  // COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_COPY_H_
