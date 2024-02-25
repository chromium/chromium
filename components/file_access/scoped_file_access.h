// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_H_
#define COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_H_

#include "base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/files/scoped_file.h"
#endif

namespace file_access {

// Move-only object to handle access token to open files.
// After the object destruction, the caller may lose access to open some of the
// requested files.
// Platform-dependant as holds ScopedFD as a token, when supported.
class COMPONENT_EXPORT(FILE_ACCESS) ScopedFileAccess {
 public:
  ScopedFileAccess(ScopedFileAccess&& other);
  ScopedFileAccess& operator=(ScopedFileAccess&& other);
  ScopedFileAccess(const ScopedFileAccess&) = delete;
  ScopedFileAccess& operator=(const ScopedFileAccess&) = delete;
  virtual ~ScopedFileAccess();

  bool is_allowed() const { return allowed_; }

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  ScopedFileAccess(bool allowed, base::ScopedFD fd);
#else
  explicit ScopedFileAccess(bool allowed);
#endif

  // Object identifying allowed access.
  static ScopedFileAccess Allowed();

  // Object identifying denied access.
  static ScopedFileAccess Denied();

 private:
  bool allowed_;
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // Holds access token. When closed, access may be revoked.
  base::ScopedFD lifeline_fd_;
#endif
};

}  // namespace file_access

#endif  // COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_H_
