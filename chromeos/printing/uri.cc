// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/uri.h"

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/printing/uri_impl.h"

namespace chromeos {

namespace {

constexpr unsigned char kFirstPrintableChar = 32;
constexpr unsigned char kLastPrintableChar = 126;

// Convert an input value 0-15 to a hex digit (0-9,A-H).
char ToHexDigit(uint8_t val) {
  DCHECK_LT(val, 16);
  if (val < 10)
    return ('0' + val);
  return ('A' + val - 10);
}

// Helper class used to encode an input strings by %-escaping disallowed bytes.
class Encoder {
 public:
  // Constructor. The set of allowed characters = STD_CHARS + |additional|.
  explicit Encoder(const std::string& additional) { Allow(additional); }
  // Extends the set of allowed characters by |chars|.
  // All characters in |chars| must be printable ASCII characters.
  void Allow(const std::string& chars) {
    for (char c : chars) {
      const unsigned char uc = static_cast<unsigned char>(c);
      DCHECK_GE(uc, kFirstPrintableChar);
      DCHECK_LE(uc, kLastPrintableChar);
      allowed_[uc - kFirstPrintableChar] = true;
    }
  }
  // Removes |chars| from the set of allowed characters.
  // All characters in |chars| must be printable ASCII characters.
  void Disallow(const std::string& chars) {
    for (char c : chars) {
      const unsigned char uc = static_cast<unsigned char>(c);
      DCHECK_GE(uc, kFirstPrintableChar);
      DCHECK_LE(uc, kLastPrintableChar);
      allowed_[uc - kFirstPrintableChar] = false;
    }
  }
  // Encodes the input string |str| and appends the output string to |out|.
  // |out| cannot be nullptr. |str| may contain UTF-8 characters, but cannot
  // include ASCII characters from range [0,kFirstPrintablechar).
  void EncodeAndAppend(const std::string& str, std::string* out) const {
    for (auto it = str.begin(); it < str.end(); ++it) {
      const unsigned char uc = static_cast<unsigned char>(*it);
      DCHECK_GE(uc, kFirstPrintableChar);
      if (uc <= kLastPrintableChar && allowed_[uc - kFirstPrintableChar]) {
        out->push_back(*it);
      } else {
        out->push_back('%');
        out->push_back(ToHexDigit(uc >> 4));
        out->push_back(ToHexDigit(uc & 0x0f));
      }
    }
  }
  // Encodes the input string |str| and returns it. |str| may contain UTF-8
  // characters, but cannot include ASCII characters from range
  // [0,kFirstPrintablechar).
  std::string Encode(const std::string& str) const {
    std::string out;
    out.reserve(str.size() * 5 / 4);
    EncodeAndAppend(str, &out);
    return out;
  }

 private:
  // The array of allowed characters. The first element corresponds to ASCII
  // value 0x20 (space), the last one to 0x7E (~). Default value contains
  // STD_CHARS.
  // Clang formatting is deactivated for this piece of code.
  // clang-format off
  std::array<bool,95> allowed_ =
      //  ! " # $ % & ' ( ) * + , - . / 0 1 2 3 4 5 6 7 8 9 : ; < = > ?
      { 0,1,0,0,1,0,0,1,1,1,1,0,1,1,1,0,1,1,1,1,1,1,1,1,1,1,0,1,0,0,0,0,
      //@ A B C D E F G H I J K L M N O P Q R S T U V W X Y Z [ \ ] ^ _
        0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,
      //` a b c d e f g h i j k l m n o p q r s t u v w x y z { | } ~
        0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1
      };
  // clang-format on
};

// Returns true if given string has characters outside ASCII (outside 0x00-07F).
bool HasNonASCII(const std::string& str) {
  return base::ranges::any_of(
      str, [](char c) { return static_cast<unsigned char>(c) > 0x7f; });
}

// The map with pairs scheme -> default_port.
const base::flat_map<std::string, int>& GetDefaultPorts() {
  static const base::NoDestructor<base::flat_map<std::string, int>>
      kDefaultPorts({{"ipp", 631},
                     {"ipps", 443},
                     {"http", 80},
                     {"https", 443},
                     {"lpd", 515},
                     {"socket", 9100}});
  return *kDefaultPorts;
}

}  //  namespace

Uri::Pim::Pim() = default;
Uri::Pim::Pim(const Pim&) = default;
Uri::Pim::~Pim() = default;

Uri::Uri() : pim_(std::make_unique<Pim>()) {}

Uri::Uri(const std::string& uri) : pim_(std::make_unique<Pim>()) {
  // Omits leading and trailing whitespaces ( \r\n\t\f\v).
  const size_t prefix_size =
      uri.size() -
      base::TrimWhitespaceASCII(uri, base::TrimPositions::TRIM_LEADING).size();
  if (prefix_size == uri.size())
    return;
  const size_t suffix_size =
      uri.size() -
      base::TrimWhitespaceASCII(uri, base::TrimPositions::TRIM_TRAILING).size();
  // Runs the parser.
  pim_->ParseUri(uri.begin() + prefix_size, uri.end() - suffix_size);
  pim_->parser_error().parsed_chars += prefix_size;
}

// static
int Uri::GetDefaultPort(const std::string& scheme) {
  auto it = GetDefaultPorts().find(scheme);
  return it != GetDefaultPorts().end() ? it->second : -1;
}

Uri::Uri(const Uri& uri) : pim_(std::make_unique<Pim>(*uri.pim_)) {}

Uri::Uri(Uri&& uri) : pim_(std::make_unique<Pim>()) {
  pim_.swap(uri.pim_);
}

Uri::~Uri() = default;

Uri& Uri::operator=(const Uri& uri) {
  *pim_ = *uri.pim_;
  return *this;
}

Uri& Uri::operator=(Uri&& uri) {
  pim_.swap(uri.pim_);
  return *this;
}

const Uri::ParserError& Uri::GetLastParsingError() const {
  return pim_->parser_error();
}

// If the Port is unspecified (==-1) it is not included in the output URI.
// If the Port is specified, it is included in the output URI <=> at least one
// of the following conditions is true:
// - |always_print_port| == true
// - Scheme has no default port number
// - Port is different than Scheme's default port number.
std::string Uri::GetNormalized(bool always_print_port) const {
  // Calculates a string representation of the Port number.
  std::string port;
  if (ShouldPrintPort(always_print_port))
    port = base::NumberToString(pim_->port());

  // Output string. Adds Scheme.
  std::string out = pim_->scheme();
  if (!out.empty())
    out.push_back(':');

  // Adds authority (Userinfo + Host + Port) if non-empty.
  Encoder enc("+&=:");
  if (!(pim_->userinfo().empty() && pim_->host().empty() && port.empty())) {
    out.append("//");
    // Userinfo.
    if (!pim_->userinfo().empty()) {
      enc.EncodeAndAppend(pim_->userinfo(), &out);
      out.push_back('@');
    }
    // Host.
    enc.Disallow(":");
    enc.EncodeAndAppend(pim_->host(), &out);
    // Port.
    if (!port.empty()) {
      out.push_back(':');
      out.append(port);
    }
  }

  // Adds Path.
  enc.Allow(":@");
  for (auto& segment : pim_->path()) {
    out.push_back('/');
    enc.EncodeAndAppend(segment, &out);
  }

  // Adds Query.
  enc.Disallow("+&=");
  enc.Allow("/?");
  for (auto it = pim_->query().begin(); it != pim_->query().end(); ++it) {
    if (it == pim_->query().begin()) {
      out.push_back('?');
    } else {
      out.push_back('&');
    }
    enc.EncodeAndAppend(it->first, &out);
    if (!it->second.empty()) {
      out.push_back('=');
      enc.EncodeAndAppend(it->second, &out);
    }
  }

  // Adds Fragment.
  enc.Allow("+&=");
  if (!pim_->fragment().empty()) {
    out.push_back('#');
    enc.EncodeAndAppend(pim_->fragment(), &out);
  }

  return out;
}

bool Uri::IsASCII() const {
  if (HasNonASCII(pim_->userinfo()) || HasNonASCII(pim_->host()) ||
      HasNonASCII(pim_->fragment())) {
    return false;
  }
  for (auto& s : pim_->path()) {
    if (HasNonASCII(s))
      return false;
  }
  for (auto& p : pim_->query()) {
    if (HasNonASCII(p.first) || HasNonASCII(p.second))
      return false;
  }
  return true;
}

std::string Uri::GetScheme() const {
  return pim_->scheme();
}

bool Uri::SetScheme(const std::string& val) {
  return pim_->ParseScheme(val.begin(), val.end());
}

int Uri::GetPort() const {
  return pim_->port();
}

bool Uri::SetPort(int val) {
  return pim_->SavePort(val);
}

bool Uri::SetPort(const std::string& port) {
  return pim_->ParsePort(port.begin(), port.end());
}

std::string Uri::GetUserinfo() const {
  return pim_->userinfo();
}
std::string Uri::GetHost() const {
  return pim_->host();
}
std::vector<std::string> Uri::GetPath() const {
  return pim_->path();
}
std::vector<std::pair<std::string, std::string>> Uri::GetQuery() const {
  return pim_->query();
}
std::string Uri::GetFragment() const {
  return pim_->fragment();
}
base::flat_map<std::string, std::vector<std::string>> Uri::GetQueryAsMap()
    const {
  base::flat_map<std::string, std::vector<std::string>> output;
  for (const auto& [key, value] : pim_->query()) {
    output[key].push_back(value);
  }
  return output;
}

std::string Uri::GetUserinfoEncoded() const {
  Encoder enc("+&=:");
  return enc.Encode(pim_->userinfo());
}
std::string Uri::GetHostEncoded() const {
  Encoder enc("+&=");
  return enc.Encode(pim_->host());
}
std::vector<std::string> Uri::GetPathEncoded() const {
  Encoder enc("+&=:@");
  std::vector<std::string> out(pim_->path().size());
  for (size_t i = 0; i < out.size(); ++i)
    out[i] = enc.Encode(pim_->path()[i]);
  return out;
}
std::string Uri::GetPathEncodedAsString() const {
  Encoder enc("+&=:@");
  std::string out;
  for (auto& segment : pim_->path())
    out += "/" + enc.Encode(segment);
  return out;
}
std::vector<std::pair<std::string, std::string>> Uri::GetQueryEncoded() const {
  Encoder enc(":@/?");
  std::vector<std::pair<std::string, std::string>> out(pim_->query().size());
  for (size_t i = 0; i < out.size(); ++i) {
    out[i].first = enc.Encode(pim_->query()[i].first);
    out[i].second = enc.Encode(pim_->query()[i].second);
  }
  return out;
}
std::string Uri::GetQueryEncodedAsString() const {
  Encoder enc(":@/?");
  std::string out;
  for (auto& param_value : pim_->query()) {
    if (!out.empty())
      out.push_back('&');
    out += enc.Encode(param_value.first);
    if (!param_value.second.empty())
      out += "=" + enc.Encode(param_value.second);
  }
  return out;
}
std::string Uri::GetFragmentEncoded() const {
  Encoder enc("+&=:@/?");
  return enc.Encode(pim_->fragment());
}

bool Uri::SetUserinfo(const std::string& val) {
  return pim_->SaveUserinfo<false>(val);
}
bool Uri::SetHost(const std::string& val) {
  return pim_->SaveHost<false>(val);
}
bool Uri::SetPath(const std::vector<std::string>& val) {
  return pim_->SavePath<false>(val);
}
bool Uri::SetQuery(
    const std::vector<std::pair<std::string, std::string>>& val) {
  return pim_->SaveQuery<false>(val);
}
bool Uri::SetFragment(const std::string& val) {
  return pim_->SaveFragment<false>(val);
}

bool Uri::SetUserinfoEncoded(const std::string& val) {
  return pim_->SaveUserinfo<true>(val);
}
bool Uri::SetHostEncoded(const std::string& val) {
  return pim_->SaveHost<true>(val);
}
bool Uri::SetPathEncoded(const std::vector<std::string>& val) {
  return pim_->SavePath<true>(val);
}
bool Uri::SetPathEncoded(const std::string& val) {
  return pim_->ParsePath(val.begin(), val.end());
}
bool Uri::SetQueryEncoded(
    const std::vector<std::pair<std::string, std::string>>& val) {
  return pim_->SaveQuery<true>(val);
}
bool Uri::SetQueryEncoded(const std::string& val) {
  return pim_->ParseQuery(val.begin(), val.end());
}
bool Uri::SetFragmentEncoded(const std::string& val) {
  return pim_->SaveFragment<true>(val);
}

bool Uri::operator<(const Uri& uri) const {
  if (pim_->scheme() < uri.pim_->scheme())
    return true;
  if (pim_->scheme() > uri.pim_->scheme())
    return false;
  if (pim_->userinfo() < uri.pim_->userinfo())
    return true;
  if (pim_->userinfo() > uri.pim_->userinfo())
    return false;
  if (pim_->host() < uri.pim_->host())
    return true;
  if (pim_->host() > uri.pim_->host())
    return false;
  if (pim_->port() < uri.pim_->port())
    return true;
  if (pim_->port() > uri.pim_->port())
    return false;
  if (pim_->path() < uri.pim_->path())
    return true;
  if (pim_->path() > uri.pim_->path())
    return false;
  if (pim_->query() < uri.pim_->query())
    return true;
  if (pim_->query() > uri.pim_->query())
    return false;
  return (pim_->fragment() < uri.pim_->fragment());
}

bool Uri::operator==(const Uri& uri) const {
  if (pim_->scheme() != uri.pim_->scheme())
    return false;
  if (pim_->userinfo() != uri.pim_->userinfo())
    return false;
  if (pim_->host() != uri.pim_->host())
    return false;
  if (pim_->port() != uri.pim_->port())
    return false;
  if (pim_->path() != uri.pim_->path())
    return false;
  if (pim_->query() != uri.pim_->query())
    return false;
  return (pim_->fragment() == uri.pim_->fragment());
}

bool Uri::ShouldPrintPort(bool always_print_port) const {
  if (pim_->port() < 0)
    return false;

  if (always_print_port)
    return true;

  auto it = GetDefaultPorts().find(pim_->scheme());
  if (it == GetDefaultPorts().end())
    return true;

  return it->second != pim_->port();
}

}  // namespace chromeos
