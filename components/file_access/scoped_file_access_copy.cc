// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/scoped_file_access_copy.h"

#include <memory>

#include "base/functional/callback_helpers.h"

namespace file_access {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
ScopedFileAccessCopy::ScopedFileAccessCopy(bool allowed,
                                           base::ScopedFD fd,
                                           base::OnceClosure copy_end_callback)
    : ScopedFileAccess(allowed, std::move(fd)),
      copy_end_callback_(
          base::ScopedClosureRunner(std::move(copy_end_callback))) {}
#else
ScopedFileAccessCopy::ScopedFileAccessCopy(bool allowed,
                                           base::OnceClosure copy_end_callback)
    : ScopedFileAccess(allowed),
      copy_end_callback_(
          base::ScopedClosureRunner(std::move(copy_end_callback))) {}
#endif
ScopedFileAccessCopy::ScopedFileAccessCopy(ScopedFileAccessCopy&& other) =
    default;
ScopedFileAccessCopy& ScopedFileAccessCopy::operator=(
    ScopedFileAccessCopy&& other) = default;
ScopedFileAccessCopy::~ScopedFileAccessCopy() = default;
}  // namespace file_access