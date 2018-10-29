// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/quarantine/quarantine.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/download/quarantine/common_linux.h"
#include "url/gurl.h"

namespace download {

namespace {

bool SetExtendedFileAttribute(const char* path,
                              const char* name,
                              const char* value,
                              size_t value_size,
                              int flags) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  int result = setxattr(path, name, value, value_size, flags);
  if (result) {
    DPLOG(ERROR) << "Could not set extended attribute " << name << " on file "
                 << path;
    return false;
  }
  return true;
}

}  // namespace

QuarantineFileResult QuarantineFile(const base::FilePath& file,
                                    const GURL& source_url,
                                    const GURL& referrer_url,
                                    const std::string& client_guid) {
  bool source_succeeded =
      source_url.is_valid() &&
      SetExtendedFileAttribute(file.value().c_str(), kSourceURLExtendedAttrName,
                               source_url.spec().c_str(),
                               source_url.spec().length(), 0);

  // Referrer being empty is not considered an error. This could happen if the
  // referrer policy resulted in an empty referrer for the download request.
  bool referrer_succeeded =
      !referrer_url.is_valid() ||
      SetExtendedFileAttribute(
          file.value().c_str(), kReferrerURLExtendedAttrName,
          referrer_url.spec().c_str(), referrer_url.spec().length(), 0);
  return source_succeeded && referrer_succeeded
             ? QuarantineFileResult::OK
             : QuarantineFileResult::ANNOTATION_FAILED;
}

}  // namespace download
