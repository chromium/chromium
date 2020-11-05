// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_FILE_HELPER_H_
#define COMPONENTS_EXO_FILE_HELPER_H_

#include <string>
#include <vector>

#include "base/callback.h"

class GURL;

namespace base {
class FilePath;
class Pickle;
}

namespace exo {

class FileHelper {
 public:
  virtual ~FileHelper() {}

  // Returns mime type which is used for list of Uris returned by this
  // FileHelper.
  virtual std::string GetMimeTypeForUriList() const = 0;

  // Converts native file path to URL which can be used by application with
  // |app_id|.  We don't expose enter file system to a container directly.
  // Instead we mount specific directory in the containers' namespace.  Thus we
  // need to convert native path to file URL which points mount point in
  // containers.  The conversion should be container specific, now we only have
  // ARC container though.
  virtual bool GetUrlFromPath(const std::string& app_id,
                              const base::FilePath& path,
                              GURL* out) = 0;

  // Takes in |pickle| constructed by the web contents view and returns true if
  // it contains any valid filesystem URLs.
  virtual bool HasUrlsInPickle(const base::Pickle& pickle) = 0;

  using UrlsFromPickleCallback =
      base::OnceCallback<void(const std::vector<GURL>& urls)>;

  // Takes in |pickle| constructed by the web contents view, reads filesystem
  // URLs from it and converts the URLs to something that applications can
  // understand.  e.g. content:// URI for Android apps.
  virtual void GetUrlsFromPickle(const std::string& app_id,
                                 const base::Pickle& pickle,
                                 UrlsFromPickleCallback callback) = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_FILE_HELPER_H_
