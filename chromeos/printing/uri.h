// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_URI_H_
#define CHROMEOS_PRINTING_URI_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"

namespace chromeos {

// This is a simple URI builder/parser.
// This class has similar functionality as GURL (Google's URL parsing library).
// However, we were not able to use GURL because of the following reasons:
// - GURL has no support for ipp/ipps scheme
// - we need general parser and builder for http-like URIs with schemes
//   different than http/https (expressions like scheme://host/path?query)
// - we need simple methods for replacing particular components (by SetX/GetX)
// - we do not care too much about edge cases, like empty Host or Path
//
// This class is a container for general http-like URI. It can parse any
// reasonable formatted http-like URI (see the grammar below) and return
// the normalized form. Valid UTF-8 characters and the escape character % are
// supported and normalized according to the rules specified in the standard
// https://tools.ietf.org/html/std66. While the general syntax of the URI is
// enforced, this class does no validate semantics of the URI. It means that
// you can freely set/modify every component with Set*(...) methods.
//
//
// General Rules
//===============
//
// The URI consists of the following components:
//  * Scheme
//  * Userinfo
//  * Host
//  * Port
//  * Path
//  * Query
//  * Fragment
// Objects of this class do not store original (input) strings. All parsed data
// is stored and returned in a normalized form. The syntax and the normalization
// algorithm is based on https://tools.ietf.org/html/std66 with the following
// modifications:
//
// 1. The grammar is simplified:
//
//    uri = [ Scheme ":" ] [ authority ] [ Path ] [ "?" Query ] [ "#" Fragment ]
//
//    authority = "//" [ Userinfo "@" ] Host [ ":" Port ]
//
//    The grammar is written in ABNF notation (RFC2234). Square brackets ([...])
//    means optional.
//
// 2. The empty Scheme/Userinfo/Host/Path/Query/Fragment is treated the same way
//    as "not specified", e.g.:
//
//      http:///a  =  http:///a?  =  http:/a?  =  http:/a#  =  http:/a
//
// 3. Relative paths are not supported. Path must be empty or start with '/'.
//
// 4. Non-printable ASCII characters (0x00-0x1F and 0x7F-0xFF) are not
//    supported, even when coded as %-escaped characters. The only exceptions
//    are bytes coding UTF-8 characters.
//
//
// Example
//=========
//
// Let's say that we want to parse an URI:
//   Uri uri("ipp://home.net:1234/my/printer/");
//   if (uri.GetLastParsingError().status != Uri::ParserStatus::kNoErrors) {
//     std::cout << "Invalid URI" << std::endl;
//   } else {
//     std::cout << "Normalized form: " << uri.GetNormalized() << std::endl;
//     std::cout << "Scheme: " << uri.GetScheme() << std::endl;
//     std::cout << "Host: " << uri.GetHost() << std::endl;
//     std::cout << "Port: " << uri.GetPort() << std::endl;
//     std::cout << "Path: " << uri.GetPath() << std::endl;
//   }
//   // Change Port to the default one:
//   uri.SetPort(-1);
//   // Change Path "/ipp/printer"
//   uri.SetPath({"ipp", "printer"});
//
//
// Default Port Numbers
//======================
//
// Some schemes have default port number. This Port number is set automatically
// when both of the following conditions are met:
//  * the current Port number is unspecified (equals -1)
//  * the Scheme equals to one of schemes from the list below
// The following schemes are recognize and has a default port number:
//  * http   :   80
//  * https  :  443
//  * ipp    :  631
//  * ipps   :  443
//  * lpd    :  515
//  * socket : 9100
//
//
// Encoding
//==========
//
// The parser accepts valid UTF-8 characters and %-escaped characters. In the
// normalized form, all bytes coding UTF-8 characters are coded as %-escaped
// characters. The components Scheme and Port do not allow for %-escaped and
// UTF-8characters. See the section Components below for details.
//
// By %-escaped character we understand here a single byte coded as three ASCII
// characters: the percent sign ('%') and two hex digits coding the value. Both
// lowercase and uppercase letters may be used as a hex digit on the input, but
// they are always normalized to uppercase letters.
//
// In general, non-printable ASCII characters are not allowed, even as %-escaped
// characters. After decoding %-escaped characters, the parser applies the
// following criteria:
//  * 0x00-0x1F - a disallowed ASCII character
//  * 0x20-0x7E - a valid ASCII character
//  * 0x7F-0xBF - a disallowed ASCII character
//  * 0xC0-0xF7 - the beginning of UTF-8 character (try to parse UTF-8 sequence)
//  * 0xF8-0xFF - a disallowed ASCII character
//
//
// Components
//============
//
// These three sets of ASCII characters are used in components definitions:
// * ALPHA - any letter (A-Z or a-z)
// * DIGIT - any digit (0-9)
// * STD_CHARS = ALPHA | DIGIT | "-" | "." | "_" | "~" | "!" | "$" | "'"
//             | "(" | ")" | "*" | "," | ";"
//
// These three properties are used in components' descriptions:
//  * Allowed characters - a set of characters that is allowed in
//        the normalized form of the component
//  * %-escaped characters - if NO, then %-escaped characters are not allowed,
//        neither on the input nor in the normalized form; it also means that
//        only characters from the "Allowed characters" property are allowed
//        on the input
//  * Case-sensitive - if NO, then lowercase and uppercase letters have the
//        same meaning and they are adjusted by the normalization algorithm
//
// Scheme
//--------
// The first character must be ALPHA.
// Allowed characters  : ALPHA | DIGIT | "+" | "-" | "."
// %-escaped characters: NO
// Case-sensitive      : NO - normalized to lowercase
//
// Userinfo
//----------
// Allowed characters  : STD_CHARS | "+" | "&" | "=" | ":"
// %-escaped characters: YES
// Case-sensitive      : YES
//
// Host
//------
// Allowed characters  : STD_CHARS | "+" | "&" | "="
// %-escaped characters: YES
// Case-sensitive      : NO - normalized to lowercase
//
// Port
//------
// It is a non-negative number; it cannot be larger than 65535.
// If not-specified and the Scheme has default Port number then the default
// number is set. In normalized URI, the Port is omitted if it equals default
// port from the Scheme. Allowed characters  : DIGIT
// %-escaped characters: NO
//
// Path
//------
// It must match to the following grammar:
//   Path = "/" segment [ Path ] | "/"
// Path equals "/" is normalized to empty Path.
// Segments "." and ".." are special and are reduced during normalization, e.g:
//   /abac/./123/def/../x  ->  /abac/123/x
//   /xzy/../../sss/  ->  /../sss
// Segment is a non-empty string with the following properties:
// Allowed characters  : STD_CHARS | "+" | "&" | "=" | ":" | "@"
// %-escaped characters: YES
// Case-sensitive      : YES
//
// Query
//-------
// It must match to the following grammar:
//   Query = [ pairs [ "&" ] ]
//   pairs = pair [ "&" pairs ]
//   pair = name [ "=" value ]
// All " " (spaces) in parsed Name and Value can be encoded as "+". However, in
// the normalized form all " " (spaces) are always encoded as %20.
// Name cannot be empty. When Value is empty, the separator "=" is omitted in
// the normalized form.
// Name and Value are strings with the following properties:
// Allowed characters  : STD_CHARS | ":" | "@" | "/" | "?"
// %-escaped characters: YES
// Case-sensitive      : YES
//
// Fragment
//----------
// Allowed characters  : STD_CHARS | "+" | "&" | "=" | ":" | "@" | "/" | "?"
// %-escaped characters: YES
// Case-sensitive      : YES
//

class COMPONENT_EXPORT(CHROMEOS_PRINTING) Uri {
 public:
  enum class ParserStatus {
    kNoErrors,
    kInvalidPercentEncoding,    // cannot parse hex number after % sign
    kDisallowedASCIICharacter,  // non-printable ASCII character
    kInvalidUTF8Character,      // error when tried to parse UTF-8 character
    kInvalidScheme,             // invalid Scheme format
    kInvalidPortNumber,
    kRelativePathsNotAllowed,  // non-empty Path that does not start with '/'
    kEmptySegmentInPath,
    kEmptyParameterNameInQuery
  };

  // This struct contains the last parser error. The parser error is always
  // set/reset by the following methods:
  // - the constructor with a parameter
  // - Set*(...) methods
  // - Set*Encoded(...) methods
  // The parser stops on the first error and reports its position in
  // |parsed_chars| as a number of successfully parsed characters from the
  // string given on the input. Methods SetQuery(...), SetQueryEncoded(...),
  // SetPath(...) and SetPathEncoded(...) may take as a parameter more than
  // one string. For them, the parser reports the number of successfully
  // parsed strings in |parsed_strings| and the position of the error in the
  // invalid string as |parsed_chars|.
  // If |status| == kNoErrors, values of the fields |parsed_chars| and
  // |parsed_strings| are undefined.
  struct ParserError {
    ParserStatus status = ParserStatus::kNoErrors;
    // The position in the input string where the parser error occurred.
    // When an error occurred for %-escaped character, it is the position of
    // the corresponding '%' sign.
    // If |status| == kNoErrors, then this value is undefined.
    size_t parsed_chars = 0;
    // This field is relevant only for the methods SetQuery(...),
    // SetPath(...), SetQueryEncoded(...) and SetPathEncoded(...).
    // In case of a parser error, it holds the number of successfully
    // parsed strings. For SetPath*(...) methods, it is an index of the
    // invalid string in the input vector. For SetQuery*(...) methods, the
    // index of invalid pair is (|parsed_strings|/2) and the value of
    // (|parsed_strings|%2) indicates the invalid string in the pair.
    // If |status| == kNoErrors, then this value is undefined.
    size_t parsed_strings = 0;
  };

  // Returns the default port number for given |scheme|. If |scheme| is not
  // known or it does not have a default port number, this method returns -1.
  static int GetDefaultPort(const std::string& scheme);

  // Constructor, creates an empty URI.
  Uri();

  // Constructor, it tries to parse |uri|.
  // Leading and trailing whitespaces (space, \t, \n, \r, \f, \v) are ignored.
  explicit Uri(const std::string& uri);

  Uri(const Uri&);
  Uri(Uri&&);
  ~Uri();

  Uri& operator=(const Uri&);
  Uri& operator=(Uri&&);

  // Returns the last parser error. The parser error is set/reset by the
  // following methods:
  // - the constructor with parameter
  // - Set*(...) methods
  // - Set*Encoded(...) methods
  const ParserError& GetLastParsingError() const;

  // Returns the URL in the normalized form. It returns empty string if and only
  // if all components are empty (see the grammar).
  // If the Port is specified (GetPort() != -1) and |always_print_port| is set
  // to true, a Port number is always included in the returned URI (even when
  // it equals to a Scheme's default port number).
  std::string GetNormalized(bool always_print_port = true) const;

  // Returns true <=> whole URL has no UTF-8 characters.
  bool IsASCII() const;

  // Returns the Scheme. Scheme cannot have %-escaped or UTF-8 characters.
  std::string GetScheme() const;

  // Sets Scheme. When the new Scheme has a default port value and the current
  // Port value is non-specified (=-1), the Port is set to the default value.
  // Scheme cannot have %-escaped or UTF-8 characters.
  // Returns false when |scheme| is invalid. In this case, the current Scheme
  // is not modified.
  bool SetScheme(const std::string& scheme);

  // Returns the Port number or -1 if the Port number is not specified.
  int GetPort() const;

  // Sets Port. |port| must be from the interval [-1,65535]. -1 means
  // "not-specified". If the current Scheme has a default port value, setting
  // -1 results in setting the default port value from the Scheme.
  // Returns false when |port| is invalid. In this case, the current port is
  // not modified.
  bool SetPort(int port);
  // A version of the method above for a string parameter. Empty string means
  // "not-specified" and has the same effect as passing -1 to the method above.
  bool SetPort(const std::string& port);

  // These methods return values of components. There is no %-escaped sequences
  // and returned string may contain UTF-8 characters.
  std::string GetUserinfo() const;
  std::string GetHost() const;
  std::vector<std::string> GetPath() const;
  std::vector<std::pair<std::string, std::string>> GetQuery() const;
  std::string GetFragment() const;
  // In the returned flat_map, vectors are never empty.
  base::flat_map<std::string, std::vector<std::string>> GetQueryAsMap() const;

  // These methods are similar to aforementioned Get* methods. The only
  // difference is that all strings are %-escaped according to the
  // normalization rules. In other words, returned values are the same as
  // in the normalized URI form returned by GetNormalized().
  std::string GetUserinfoEncoded() const;
  std::string GetHostEncoded() const;
  std::vector<std::string> GetPathEncoded() const;
  std::string GetPathEncodedAsString() const;
  std::vector<std::pair<std::string, std::string>> GetQueryEncoded() const;
  std::string GetQueryEncodedAsString() const;
  std::string GetFragmentEncoded() const;

  // These methods set value of a component. They DO NOT interpret % as an
  // escape character. Input strings may contain UTF-8 characters.
  // Returned value has the following meaning:
  // - true - no parser errors => the component was set to a new value
  // - false - a parser error occurred => no changes were made to the component
  // Every call to one of these methods resets the state returned by the method
  // GetLastParsingError(...).
  bool SetUserinfo(const std::string&);
  bool SetHost(const std::string&);
  bool SetPath(const std::vector<std::string>&);
  bool SetQuery(const std::vector<std::pair<std::string, std::string>>&);
  bool SetFragment(const std::string&);

  // These methods are similar to aforementioned Set* methods. The only
  // difference is that the DO interpret % as an escape character. UTF-8
  // characters are still allowed.
  bool SetUserinfoEncoded(const std::string&);
  bool SetHostEncoded(const std::string&);
  bool SetPathEncoded(const std::vector<std::string>&);
  bool SetPathEncoded(const std::string&);
  bool SetQueryEncoded(const std::vector<std::pair<std::string, std::string>>&);
  bool SetQueryEncoded(const std::string&);
  bool SetFragmentEncoded(const std::string&);

  // <=> operators. The order is determined by ASCII-wise comparison of the
  // vector of components (GetScheme(),GetUserinfo(),GetHost(),GetPort(),
  // GetPath(),GetQuery(),GetFragment()). The value of GetLastParsingError()
  // is not taken into account during comparison (URIs with the same components
  // but different ParserError are ==).
  bool operator<(const Uri& uri) const;
  bool operator<=(const Uri& uri) const { return !(uri < *this); }
  bool operator>(const Uri& uri) const { return (uri < *this); }
  bool operator>=(const Uri& uri) const { return !(*this < uri); }
  bool operator==(const Uri& uri) const;
  bool operator!=(const Uri& uri) const { return !(*this == uri); }

 private:
  class Pim;

  bool ShouldPrintPort(bool always_print_port) const;

  std::unique_ptr<Pim> pim_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_URI_H_
