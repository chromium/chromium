// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This is a copy of net/base/ip_address.cc circa 2023. It should be used only
// by components/feedback/redaction_tool/. We need a copy because the
// components/feedback/redaction_tool source code is shared into ChromeOS and
// needs to have no dependencies outside of base/.

#include "components/feedback/redaction_tool/ip_address.h"

#include <algorithm>
#include <climits>
#include <string_view>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "components/feedback/redaction_tool/url_canon_ip.h"
#include "components/feedback/redaction_tool/url_canon_stdstring.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace redaction_internal {
namespace {

// The prefix for IPv6 mapped IPv4 addresses.
// https://tools.ietf.org/html/rfc4291#section-2.5.5.2
constexpr uint8_t kIPv4MappedPrefix[] = {0, 0, 0, 0, 0,    0,
                                         0, 0, 0, 0, 0xFF, 0xFF};

// Note that this function assumes:
// * |ip_address| is at least |prefix_length_in_bits| (bits) long;
// * |ip_prefix| is at least |prefix_length_in_bits| (bits) long.
bool IPAddressPrefixCheck(const IPAddressBytes& ip_address,
                          const uint8_t* ip_prefix,
                          size_t prefix_length_in_bits) {
  // Compare all the bytes that fall entirely within the prefix.
  size_t num_entire_bytes_in_prefix = prefix_length_in_bits / 8;
  for (size_t i = 0; i < num_entire_bytes_in_prefix; ++i) {
    if (ip_address[i] != ip_prefix[i]) {
      return false;
    }
  }

  // In case the prefix was not a multiple of 8, there will be 1 byte
  // which is only partially masked.
  size_t remaining_bits = prefix_length_in_bits % 8;
  if (remaining_bits != 0) {
    uint8_t mask = 0xFF << (8 - remaining_bits);
    size_t i = num_entire_bytes_in_prefix;
    if ((ip_address[i] & mask) != (ip_prefix[i] & mask)) {
      return false;
    }
  }
  return true;
}

bool ParseIPLiteralToBytes(std::string_view ip_literal, IPAddressBytes* bytes) {
  // |ip_literal| could be either an IPv4 or an IPv6 literal. If it contains
  // a colon however, it must be an IPv6 address.
  if (ip_literal.find(':') != std::string_view::npos) {
    // GURL expects IPv6 hostnames to be surrounded with brackets.
    std::string host_brackets = base::StrCat({"[", ip_literal, "]"});
    Component host_comp(0, host_brackets.size());

    // Try parsing the hostname as an IPv6 literal.
    bytes->Resize(16);  // 128 bits.
    return IPv6AddressToNumber(host_brackets.data(), host_comp, bytes->data());
  }

  // Otherwise the string is an IPv4 address.
  bytes->Resize(4);  // 32 bits.
  Component host_comp(0, ip_literal.size());
  int num_components;
  CanonHostInfo::Family family = IPv4AddressToNumber(
      ip_literal.data(), host_comp, bytes->data(), &num_components);
  return family == CanonHostInfo::IPV4;
}

}  // namespace

IPAddressBytes::IPAddressBytes() : size_(0) {}

IPAddressBytes::IPAddressBytes(const uint8_t* data, size_t data_len) {
  Assign(data, data_len);
}

IPAddressBytes::~IPAddressBytes() = default;
IPAddressBytes::IPAddressBytes(IPAddressBytes const& other) = default;

void IPAddressBytes::Assign(const uint8_t* data, size_t data_len) {
  size_ = data_len;
  CHECK_GE(16u, data_len);
  std::copy_n(data, data_len, bytes_.data());
}

bool IPAddressBytes::operator<(const IPAddressBytes& other) const {
  if (size_ == other.size_) {
    return std::lexicographical_compare(begin(), end(), other.begin(),
                                        other.end());
  }
  return size_ < other.size_;
}

bool IPAddressBytes::operator==(const IPAddressBytes& other) const {
  return base::ranges::equal(*this, other);
}

bool IPAddressBytes::operator!=(const IPAddressBytes& other) const {
  return !(*this == other);
}

// static

IPAddress::IPAddress() = default;

IPAddress::IPAddress(const IPAddress& other) = default;

IPAddress::IPAddress(const IPAddressBytes& address) : ip_address_(address) {}

IPAddress::IPAddress(const uint8_t* address, size_t address_len)
    : ip_address_(address, address_len) {}

IPAddress::IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  ip_address_.push_back(b0);
  ip_address_.push_back(b1);
  ip_address_.push_back(b2);
  ip_address_.push_back(b3);
}

IPAddress::IPAddress(uint8_t b0,
                     uint8_t b1,
                     uint8_t b2,
                     uint8_t b3,
                     uint8_t b4,
                     uint8_t b5,
                     uint8_t b6,
                     uint8_t b7,
                     uint8_t b8,
                     uint8_t b9,
                     uint8_t b10,
                     uint8_t b11,
                     uint8_t b12,
                     uint8_t b13,
                     uint8_t b14,
                     uint8_t b15) {
  ip_address_.push_back(b0);
  ip_address_.push_back(b1);
  ip_address_.push_back(b2);
  ip_address_.push_back(b3);
  ip_address_.push_back(b4);
  ip_address_.push_back(b5);
  ip_address_.push_back(b6);
  ip_address_.push_back(b7);
  ip_address_.push_back(b8);
  ip_address_.push_back(b9);
  ip_address_.push_back(b10);
  ip_address_.push_back(b11);
  ip_address_.push_back(b12);
  ip_address_.push_back(b13);
  ip_address_.push_back(b14);
  ip_address_.push_back(b15);
}

IPAddress::~IPAddress() = default;

bool IPAddress::IsIPv4() const {
  return ip_address_.size() == kIPv4AddressSize;
}

bool IPAddress::IsIPv6() const {
  return ip_address_.size() == kIPv6AddressSize;
}

bool IPAddress::IsValid() const {
  return IsIPv4() || IsIPv6();
}

bool IPAddress::IsIPv4MappedIPv6() const {
  return IsIPv6() && IPAddressStartsWith(*this, kIPv4MappedPrefix);
}

bool IPAddress::AssignFromIPLiteral(std::string_view ip_literal) {
  bool success = ParseIPLiteralToBytes(ip_literal, &ip_address_);
  if (!success) {
    ip_address_.Resize(0);
  }
  return success;
}

// static
IPAddress IPAddress::AllZeros(size_t num_zero_bytes) {
  CHECK_LE(num_zero_bytes, 16u);
  IPAddress result;
  for (size_t i = 0; i < num_zero_bytes; ++i) {
    result.ip_address_.push_back(0u);
  }
  return result;
}

// static
IPAddress IPAddress::IPv4AllZeros() {
  return AllZeros(kIPv4AddressSize);
}

// static
IPAddress IPAddress::IPv6AllZeros() {
  return AllZeros(kIPv6AddressSize);
}

bool IPAddress::operator==(const IPAddress& that) const {
  return ip_address_ == that.ip_address_;
}

bool IPAddress::operator!=(const IPAddress& that) const {
  return ip_address_ != that.ip_address_;
}

bool IPAddress::operator<(const IPAddress& that) const {
  // Sort IPv4 before IPv6.
  if (ip_address_.size() != that.ip_address_.size()) {
    return ip_address_.size() < that.ip_address_.size();
  }

  return ip_address_ < that.ip_address_;
}

std::string IPAddress::ToString() const {
  std::string str;
  StdStringCanonOutput output(&str);

  if (IsIPv4()) {
    AppendIPv4Address(ip_address_.data(), &output);
  } else if (IsIPv6()) {
    AppendIPv6Address(ip_address_.data(), &output);
  }

  output.Complete();
  return str;
}

IPAddress ConvertIPv4ToIPv4MappedIPv6(const IPAddress& address) {
  DCHECK(address.IsIPv4());
  // IPv4-mapped addresses are formed by:
  // <80 bits of zeros>  + <16 bits of ones> + <32-bit IPv4 address>.
  absl::InlinedVector<uint8_t, 16> bytes;
  bytes.insert(bytes.end(), std::begin(kIPv4MappedPrefix),
               std::end(kIPv4MappedPrefix));
  bytes.insert(bytes.end(), address.bytes().begin(), address.bytes().end());
  return IPAddress(bytes.data(), bytes.size());
}

IPAddress ConvertIPv4MappedIPv6ToIPv4(const IPAddress& address) {
  DCHECK(address.IsIPv4MappedIPv6());

  absl::InlinedVector<uint8_t, 16> bytes;
  bytes.insert(bytes.end(),
               address.bytes().begin() + std::size(kIPv4MappedPrefix),
               address.bytes().end());
  return IPAddress(bytes.data(), bytes.size());
}

bool IPAddressMatchesPrefix(const IPAddress& ip_address,
                            const IPAddress& ip_prefix,
                            size_t prefix_length_in_bits) {
  // Both the input IP address and the prefix IP address should be either IPv4
  // or IPv6.
  DCHECK(ip_address.IsValid());
  DCHECK(ip_prefix.IsValid());

  DCHECK_LE(prefix_length_in_bits, ip_prefix.size() * 8);

  // In case we have an IPv6 / IPv4 mismatch, convert the IPv4 addresses to
  // IPv6 addresses in order to do the comparison.
  if (ip_address.size() != ip_prefix.size()) {
    if (ip_address.IsIPv4()) {
      return IPAddressMatchesPrefix(ConvertIPv4ToIPv4MappedIPv6(ip_address),
                                    ip_prefix, prefix_length_in_bits);
    }
    return IPAddressMatchesPrefix(ip_address,
                                  ConvertIPv4ToIPv4MappedIPv6(ip_prefix),
                                  96 + prefix_length_in_bits);
  }

  return IPAddressPrefixCheck(ip_address.bytes(), ip_prefix.bytes().data(),
                              prefix_length_in_bits);
}

}  // namespace redaction_internal
