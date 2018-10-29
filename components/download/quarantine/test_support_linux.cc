// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/quarantine/test_support.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "components/download/quarantine/common_linux.h"
#include "url/gurl.h"

namespace download {

namespace {

std::string GetExtendedFileAttribute(const char* path, const char* name) {
  base::AssertBlockingAllowedDeprecated();
  ssize_t len = getxattr(path, name, nullptr, 0);
  if (len <= 0)
    return std::string();

  std::vector<char> buffer(len);
  len = getxattr(path, name, buffer.data(), buffer.size());
  if (len < static_cast<ssize_t>(buffer.size()))
    return std::string();
  return std::string(buffer.begin(), buffer.end());
}

}  // namespace

bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& source_url,
                       const GURL& referrer_url) {
  if (!base::PathExists(file))
    return false;

  std::string url_value = GetExtendedFileAttribute(file.value().c_str(),
                                                   kSourceURLExtendedAttrName);
  if (source_url.is_empty())
    return !url_value.empty();

  if (source_url != GURL(url_value))
    return false;

  return !referrer_url.is_valid() ||
         GURL(GetExtendedFileAttribute(file.value().c_str(),
                                       kReferrerURLExtendedAttrName)) ==
             referrer_url;
}

}  // namespace download
