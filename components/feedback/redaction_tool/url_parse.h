// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a copy of url/third_party/mozilla/url_parse.h circa 2023.
// It should be used only by components/feedback/redaction_tool/.
// We need a copy because the components/feedback/redaction_tool source code is
// shared into ChromeOS and needs to have no dependencies outside of base/.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_PARSE_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_PARSE_H_

#include <iosfwd>

#include "base/memory/raw_ptr.h"

namespace redaction_internal {

// Component ------------------------------------------------------------------

// Represents a substring for URL parsing.
struct Component {
  Component() : begin(0), len(-1) {}

  // Normal constructor: takes an offset and a length.
  Component(int b, int l) : begin(b), len(l) {}

  int end() const { return begin + len; }

  // Returns true if this component is valid, meaning the length is given.
  // Valid components may be empty to record the fact that they exist.
  bool is_valid() const { return len >= 0; }

  // Determine if the component is empty or not. Empty means the length is
  // zero or the component is invalid.
  bool is_empty() const { return len <= 0; }
  bool is_nonempty() const { return len > 0; }

  void reset() {
    begin = 0;
    len = -1;
  }

  bool operator==(const Component& other) const {
    return begin == other.begin && len == other.len;
  }

  int begin;  // Byte offset in the string of this component.
  int len;    // Will be -1 if the component is unspecified.
};

// Parsed ---------------------------------------------------------------------

// A structure that holds the identified parts of an input URL. This structure
// does NOT store the URL itself. The caller will have to store the URL text
// and its corresponding Parsed structure separately.
//
// Typical usage would be:
//
//    Parsed parsed;
//    Component scheme;
//    if (!ExtractScheme(url, url_len, &scheme))
//      return I_CAN_NOT_FIND_THE_SCHEME_DUDE;
//
//    if (IsStandardScheme(url, scheme))  // Not provided by this component
//      ParseStandardURL(url, url_len, &parsed);
//    else if (IsFileURL(url, scheme))    // Not provided by this component
//      ParseFileURL(url, url_len, &parsed);
//    else
//      ParsePathURL(url, url_len, &parsed);
//
struct Parsed {
  // Identifies different components.
  enum ComponentType {
    SCHEME,
    USERNAME,
    PASSWORD,
    HOST,
    PORT,
    PATH,
    QUERY,
    REF,
  };

  // The default constructor is sufficient for the components, but inner_parsed_
  // requires special handling.
  Parsed();
  Parsed(const Parsed&);
  Parsed& operator=(const Parsed&);
  ~Parsed();

  // Returns the length of the URL (the end of the last component).
  //
  // Note that for some invalid, non-canonical URLs, this may not be the length
  // of the string. For example "http://": the parsed structure will only
  // contain an entry for the four-character scheme, and it doesn't know about
  // the "://". For all other last-components, it will return the real length.
  int Length() const;

  // Returns the number of characters before the given component if it exists,
  // or where the component would be if it did exist. This will return the
  // string length if the component would be appended to the end.
  //
  // Note that this can get a little funny for the port, query, and ref
  // components which have a delimiter that is not counted as part of the
  // component. The |include_delimiter| flag controls if you want this counted
  // as part of the component or not when the component exists.
  //
  // This example shows the difference between the two flags for two of these
  // delimited components that is present (the port and query) and one that
  // isn't (the reference). The components that this flag affects are marked
  // with a *.
  //                 0         1         2
  //                 012345678901234567890
  // Example input:  http://foo:80/?query
  //              include_delim=true,  ...=false  ("<-" indicates different)
  //      SCHEME: 0                    0
  //    USERNAME: 5                    5
  //    PASSWORD: 5                    5
  //        HOST: 7                    7
  //       *PORT: 10                   11 <-
  //        PATH: 13                   13
  //      *QUERY: 14                   15 <-
  //        *REF: 20                   20
  //
  int CountCharactersBefore(ComponentType type, bool include_delimiter) const;

  // Scheme without the colon: "http://foo"/ would have a scheme of "http".
  // The length will be -1 if no scheme is specified ("foo.com"), or 0 if there
  // is a colon but no scheme (":foo"). Note that the scheme is not guaranteed
  // to start at the beginning of the string if there are proceeding whitespace
  // or control characters.
  Component scheme;

  // Username. Specified in URLs with an @ sign before the host. See |password|
  Component username;

  // Password. The length will be -1 if unspecified, 0 if specified but empty.
  // Not all URLs with a username have a password, as in "http://me@host/".
  // The password is separated form the username with a colon, as in
  // "http://me:secret@host/"
  Component password;

  // Host name.
  Component host;

  // Port number.
  Component port;

  // Path, this is everything following the host name, stopping at the query of
  // ref delimiter (if any). Length will be -1 if unspecified. This includes
  // the preceeding slash, so the path on http://www.google.com/asdf" is
  // "/asdf". As a result, it is impossible to have a 0 length path, it will
  // be -1 in cases like "http://host?foo".
  // Note that we treat backslashes the same as slashes.
  Component path;

  // Stuff between the ? and the # after the path. This does not include the
  // preceeding ? character. Length will be -1 if unspecified, 0 if there is
  // a question mark but no query string.
  Component query;

  // Indicated by a #, this is everything following the hash sign (not
  // including it). If there are multiple hash signs, we'll use the last one.
  // Length will be -1 if there is no hash sign, or 0 if there is one but
  // nothing follows it.
  Component ref;

  // The URL spec from the character after the scheme: until the end of the
  // URL, regardless of the scheme. This is mostly useful for 'opaque' non-
  // hierarchical schemes like data: and javascript: as a convient way to get
  // the string with the scheme stripped off.
  Component GetContent() const;

  // True if the URL's source contained a raw `<` character, and whitespace was
  // removed from the URL during parsing
  //
  // TODO(mkwst): Link this to something in a spec if
  // https://github.com/whatwg/url/pull/284 lands.
  bool potentially_dangling_markup;

  // This is used for nested URL types, currently only filesystem.  If you
  // parse a filesystem URL, the resulting Parsed will have a nested
  // inner_parsed_ to hold the parsed inner URL's component information.
  // For all other url types [including the inner URL], it will be NULL.
  Parsed* inner_parsed() const { return inner_parsed_; }

  void set_inner_parsed(const Parsed& inner_parsed) {
    if (!inner_parsed_) {
      inner_parsed_ = new Parsed(inner_parsed);
    } else {
      *inner_parsed_ = inner_parsed;
    }
  }

  void clear_inner_parsed() {
    if (inner_parsed_) {
      delete inner_parsed_;
      inner_parsed_ = nullptr;
    }
  }

 private:
  raw_ptr<Parsed>
      inner_parsed_;  // This object is owned and managed by this struct.
};

}  // namespace redaction_internal

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_PARSE_H_
