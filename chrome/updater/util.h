// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_H_
#define CHROME_UPDATER_UTIL_H_

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

// Externally-defined printers for base types.
namespace base {

class CommandLine;

template <class T>
std::ostream& operator<<(std::ostream& os, const absl::optional<T>& opt) {
  if (opt.has_value()) {
    return os << opt.value();
  } else {
    return os << "absl::nullopt";
  }
}

}  // namespace base

namespace updater {

namespace tagging {
struct TagArgs;
}

enum class UpdaterScope;

// Returns the base directory common to all versions of the updater. For
// instance, this function may return %localappdata%\Chromium\ChromiumUpdater
// for a user install.
absl::optional<base::FilePath> GetBaseDirectory(UpdaterScope scope);

// Returns a versioned directory under which the running version of the updater
// stores its files and data. For instance, this function may return
// %localappdata%\Chromium\ChromiumUpdater\1.2.3.4 for a user install.
absl::optional<base::FilePath> GetVersionedDirectory(UpdaterScope scope);

// Returns the parsed values from --tag command line argument. The function
// implementation uses lazy initialization and caching to avoid reparsing
// the tag.
absl::optional<tagging::TagArgs> GetTagArgs();

// Returns true if the user running the updater also owns the `path`.
bool PathOwnedByUser(const base::FilePath& path);

// Initializes logging for an executable.
void InitLogging(UpdaterScope updater_scope,
                 const base::FilePath::StringType& filename);

// Wraps the 'command_line' to be executed in an elevated context.
// On macOS this is done with 'sudo'.
base::CommandLine MakeElevated(base::CommandLine command_line);

// Functor used by associative containers of strings as a case-insensitive ASCII
// compare. `StringT` could be either UTF-8 or UTF-16.
struct CaseInsensitiveASCIICompare {
 public:
  template <typename StringT>
  bool operator()(const StringT& x, const StringT& y) const {
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
