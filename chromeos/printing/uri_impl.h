// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_URI_IMPL_H_
#define CHROMEOS_PRINTING_URI_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "chromeos/printing/uri.h"

// This file contains a declaration of struct used in the implementation of
// class Uri declared in uri.h. This file is not supposed to be included
// anywhere outside of the class Uri.

namespace chromeos {

using Iter = std::string::const_iterator;

class Uri::Pim {
 public:
  Pim();
  Pim(const Pim&);
  ~Pim();

  // These methods parse and normalize the corresponding component(s) from the
  // input string |begin|-|end|. Each component is saved only if successfully
  // parsed and verified. In case of an error, the field |parser_error| is set
  // and false is returned. All methods assume that |begin| <= |end|.
  // Additional notes for particular components:
  //  * Scheme: if the current Port is unspecified and the new Scheme has
  //    default port number, the Port is set to this default value
  //  * Authority: this is Userinfo + Host + Port, see description in uri.h for
  //    the grammar
  //  * Port: an empty string means -1, see the method SavePort(int) below
  //  * Path: the input string must be empty or starts from '/'
  bool ParseScheme(const Iter& begin, const Iter& end);
  bool ParseAuthority(const Iter& begin, const Iter& end);
  bool ParsePort(const Iter& begin, const Iter& end);
  bool ParsePath(const Iter& begin, const Iter& end);
  bool ParseQuery(const Iter& begin, const Iter& end);
  bool ParseFragment(const Iter& begin, const Iter& end);

  // This method parse the whole URI. It calls internally the methods
  // Parse*(...) declared above. In case of an error, the method set the field
  // |parser_error| and returns false. Parsing stops on the first error,
  // components that have been successfully parsed are saved.
  bool ParseUri(const Iter& begin, const Iter end);

  // This method fails (and return false) <=> |port| is smaller than -1 or
  // larger than 65535. If |port| == -1 and the current Scheme has a default
  // port, the default port is set as a new Port number. The field
  // |parser_error| is set accordingly.
  bool SavePort(int port);

  // These methods save values of corresponding components. The template
  // parameter |encoded| trigger resolution of %-escaped characters. If set to
  // true, every % sign in the input value is treated as the beginning of
  // %-escaped character; if set to false, % signs are treated as regular ASCII
  // characters. All input values are validated and normalized, but without
  // %-escaping fragile characters (components are stored in "native" form). In
  // case of a failure, false is returned, the value of target component is not
  // modified, and the field |parser_error| is set accordingly.
  template <bool encoded>
  bool SaveUserinfo(const std::string& val);
  template <bool encoded>
  bool SaveHost(const std::string& val);
  template <bool encoded>
  bool SavePath(const std::vector<std::string>& val);
  template <bool encoded>
  bool SaveQuery(const std::vector<std::pair<std::string, std::string>>& val);
  template <bool encoded>
  bool SaveFragment(const std::string& val);

  // Getters for all fields.
  const std::string& scheme() const { return scheme_; }
  const std::string& userinfo() const { return userinfo_; }
  const std::string& host() const { return host_; }
  int port() const { return port_; }
  const std::vector<std::string>& path() const { return path_; }
  const std::vector<std::pair<std::string, std::string>>& query() const {
    return query_;
  }
  const std::string& fragment() const { return fragment_; }

  // Access to the |parser_error_|
  ParserError& parser_error() { return parser_error_; }

 private:
  // Reads the string |begin|-|end| and perform the following operations:
  //  1. if |plus_to_space| is true, all '+' signs are converted to ' ' (space)
  //  2. if |encoded| is true, all % signs are treated as initiators of
  //     %-escaped characters and decoded to corresponding ASCII
  //  3. if |case_insensitive| is true, all capital ASCII letters are converted
  //     to lowercase
  //  4. all UTF-8 characters are validated
  //  5. all ASCII characters are validated (see the section Encoding in uri.h)
  // The output is saved to |out|. In case of an error, the method set the field
  // |parser_error| and returns false.
  // The following initial requirements must be met:
  //  * |begin| <= |end|
  //  * |out| must point to an empty string
  // When the method returns false, |out| may contain invalid value.
  template <bool encoded, bool case_insensitive = false>
  bool ParseString(const Iter& begin,
                   const Iter& end,
                   std::string* out,
                   bool plus_to_space = false);

  // Components values. They are valid and normalized, but before %-escaping.
  std::string scheme_;
  std::string userinfo_;
  std::string host_;
  int port_ = -1;  // -1 means "unspecified"
  // A list of path's segments, without separators ('/').
  std::vector<std::string> path_;
  // A list of parameters name=value; value may be empty.
  std::vector<std::pair<std::string, std::string>> query_;
  std::string fragment_;

  // The last parser status.
  ParserError parser_error_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_URI_IMPL_H_
