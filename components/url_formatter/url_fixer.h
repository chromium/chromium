// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_URL_FIXER_H_
#define COMPONENTS_URL_FORMATTER_URL_FIXER_H_

#include <string>

#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace url {
struct Component;
struct Parsed;
}

// These methods process user typed input that is meant to be a URL - like user
// typing in the URL bar or command line switches. The output is NOT guaranteed
// to be a valid URL.
//
// This is NOT the place for converting between different types of URLs or
// parsing them, see net_util.h for that. These methods should only be used on
// user typed input, NOT untrusted strings sourced from the web or elsewhere.
namespace url_formatter {

// Segments the given text string into parts of a URL. This is most useful for
// schemes such as http, https, and ftp where |SegmentURL| will find many
// segments. Currently does not segment "file" schemes.
// Returns the canonicalized scheme, or the empty string when |text| is only
// whitespace.
std::string SegmentURL(const std::string& text, url::Parsed* parts);
std::u16string SegmentURL(const std::u16string& text, url::Parsed* parts);

// Attempts to fix common problems in user-typed text, making some "smart"
// adjustments to obviously-invalid input where possible.
//
// The result can still be invalid, so check the return value's validity or
// use possibly_invalid_spec(). DO NOT USE this method on untrusted strings
// from the web or elsewhere. Only use this for user-typed input.
//
// If |text| may be an absolute path to a file, it will get converted to a
// "file:" URL.
//
// Schemes "about" and "chrome" are normalized to "chrome://", with slashes.
// "about:blank" is unaltered, as Webkit allows frames to access about:blank.
// Additionally, if a chrome URL does not have a valid host, as in "about:", the
// returned URL will have the host "version", as in "chrome://version".
//
// If |desired_tld| is non-empty, it represents the TLD the user wishes to
// append in the case of an incomplete domain. We check that this is not a file
// path and there does not appear to be a valid TLD already, then append
// |desired_tld| to the domain and prepend "www." (unless it, or a scheme, are
// already present.)  This TLD should not have a leading '.' (use "com" instead
// of ".com").
GURL FixupURL(const std::string& text, const std::string& desired_tld);

// Converts |text| to a fixed-up URL, allowing it to be a relative path on the
// local filesystem. Begin searching in |base_dir|; if empty, use the current
// working directory. If this resolves to a file on disk, convert it to a
// "file:" URL in |fixed_up_url|; otherwise, fall back to the behavior of
// FixupURL().
//
// For "regular" input, even if it is possibly a file with a full path, you
// should use FixupURL() directly. This function should only be used when
// relative path handling is desired, as for command line processing.
GURL FixupRelativeFile(const base::FilePath& base_dir,
                       const base::FilePath& text);

// Offsets the beginning index of |part| by |offset|, which is allowed to be
// negative. In some cases, the desired component does not exist at the given
// offset. For example, when converting from "http://foo" to "foo", the scheme
// component no longer exists. In such a case, the beginning index is set to 0.
// Does nothing if |part| is invalid.
void OffsetComponent(int offset, url::Component* part);

// Returns true if |scheme1| is equivalent to |scheme2|.
// Generally this is true if the two schemes are actually identical, but it's
// also true when one scheme is "about" and the other "chrome".
bool IsEquivalentScheme(const std::string& scheme1, const std::string& scheme2);

// For paths like ~, we use $HOME for the current user's home directory.
// For tests, we allow our idea of $HOME to be overriden by this variable.
extern const char* home_directory_override;

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_URL_FIXER_H_
