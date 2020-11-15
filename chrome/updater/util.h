// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_H_
#define CHROME_UPDATER_UTIL_H_

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

class GURL;

// Externally-defined printers for base types.
namespace base {

template <class T>
std::ostream& operator<<(std::ostream& os, const base::Optional<T>& opt) {
  if (opt.has_value()) {
    return os << opt.value();
  } else {
    return os << "base::nullopt";
  }
}

}  // namespace base

namespace updater {

// Returns the base directory common to all versions of the updater. For
// instance, this function may return %localappdata%\Chromium\ChromiumUpdater
// for a User install.
bool GetBaseDirectory(base::FilePath* path);

// Returns a versioned directory under which the running version of the updater
// stores its files and data. For instance, this function may return
// %localappdata%\Chromium\ChromiumUpdater\1.2.3.4 for a User install.
bool GetVersionedDirectory(base::FilePath* path);

// Returns true if the user running the updater also owns the |path|.
bool PathOwnedByUser(const base::FilePath& path);

// Initializes logging for an executable.
void InitLogging(const base::FilePath::StringType& filename);

// Functor used by associative containers of strings as a case-insensitive ASCII
// compare. |T| could be std::string or base::string16.
template <typename T>
struct CaseInsensitiveASCIICompare {
 public:
  bool operator()(base::BasicStringPiece<T> x,
                  base::BasicStringPiece<T> y) const {
    return base::CompareCaseInsensitiveASCII(x, y) > 0;
  }
};

// Returns a new GURL by appending the given query parameter name and the
// value. Unsafe characters in the name and the value are escaped like
// %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendQueryParameter(GURL("http://example.com"), "name", "value").spec()
// => "http://example.com?name=value"
// AppendQueryParameter(GURL("http://example.com?x=y"), "name", "value").spec()
// => "http://example.com?x=y&name=value"
GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_H_
