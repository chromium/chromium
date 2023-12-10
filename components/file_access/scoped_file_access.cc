// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/scoped_file_access.h"

namespace file_access {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
ScopedFileAccess::ScopedFileAccess(bool allowed, base::ScopedFD fd)
    : allowed_(allowed), lifeline_fd_(std::move(fd)) {}
#else
ScopedFileAccess::ScopedFileAccess(bool allowed) : allowed_(allowed) {}
#endif
ScopedFileAccess::ScopedFileAccess(ScopedFileAccess&& other) = default;
ScopedFileAccess& ScopedFileAccess::operator=(ScopedFileAccess&& other) =
    default;
ScopedFileAccess::~ScopedFileAccess() = default;

// static
ScopedFileAccess ScopedFileAccess::Allowed() {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return ScopedFileAccess(/*allowed=*/true, base::ScopedFD());
#else
  return ScopedFileAccess(/*allowed=*/true);
#endif
}

// static
ScopedFileAccess ScopedFileAccess::Denied() {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return ScopedFileAccess(/*allowed=*/false, base::ScopedFD());
#else
  return ScopedFileAccess(/*allowed=*/false);
#endif
}

}  // namespace file_access
