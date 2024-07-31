// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This is a copy of url/url_canon_ip.cc circa 2023. It should be used only by
// components/feedback/redaction_tool/.
// We need a copy because the components/feedback/redaction_tool source code is
// shared into ChromeOS and needs to have no dependencies outside of base/.

#include "components/feedback/redaction_tool/url_canon_ip.h"

#include <stdint.h>
#include <stdlib.h>

#include <limits>

#include "base/check.h"
#include "components/feedback/redaction_tool/url_canon_internal.h"

namespace redaction_internal {

namespace {

// Converts one of the character types that represent a numerical base to the
// corresponding base.
int BaseForType(SharedCharTypes type) {
  switch (type) {
    case CHAR_HEX:
      return 16;
    case CHAR_DEC:
      return 10;
    case CHAR_OCT:
      return 8;
    default:
      return 0;
  }
}

// Converts an IPv4 component to a 32-bit number, while checking for overflow.
//
// Possible return values:
// - IPV4    - The number was valid, and did not overflow.
// - BROKEN  - The input was numeric, but too large for a 32-bit field.
// - NEUTRAL - Input was not numeric.
//
// The input is assumed to be ASCII. The components are assumed to be non-empty.
template <typename CHAR>
CanonHostInfo::Family IPv4ComponentToNumber(const CHAR* spec,
                                            const Component& component,
                                            uint32_t* number) {
  // Empty components are considered non-numeric.
  if (component.is_empty()) {
    return CanonHostInfo::NEUTRAL;
  }

  // Figure out the base
  SharedCharTypes base;
  int base_prefix_len = 0;  // Size of the prefix for this base.
  if (spec[component.begin] == '0') {
    // Either hex or dec, or a standalone zero.
    if (component.len == 1) {
      base = CHAR_DEC;
    } else if (spec[component.begin + 1] == 'X' ||
               spec[component.begin + 1] == 'x') {
      base = CHAR_HEX;
      base_prefix_len = 2;
    } else {
      base = CHAR_OCT;
      base_prefix_len = 1;
    }
  } else {
    base = CHAR_DEC;
  }

  // Extend the prefix to consume all leading zeros.
  while (base_prefix_len < component.len &&
         spec[component.begin + base_prefix_len] == '0') {
    base_prefix_len++;
  }

  // Put the component, minus any base prefix, into a NULL-terminated buffer so
  // we can call the standard library. Because leading zeros have already been
  // discarded, filling the entire buffer is guaranteed to trigger the 32-bit
  // overflow check.
  const int kMaxComponentLen = 16;
  char buf[kMaxComponentLen + 1];  // digits + '\0'
  int dest_i = 0;
  bool may_be_broken_octal = false;
  for (int i = component.begin + base_prefix_len; i < component.end(); i++) {
    if (spec[i] >= 0x80) {
      return CanonHostInfo::NEUTRAL;
    }

    // We know the input is 7-bit, so convert to narrow (if this is the wide
    // version of the template) by casting.
    char input = static_cast<char>(spec[i]);

    // Validate that this character is OK for the given base.
    if (!IsCharOfType(input, base)) {
      if (IsCharOfType(input, CHAR_DEC)) {
        // Entirely numeric components with leading 0s that aren't octal are
        // considered broken.
        may_be_broken_octal = true;
      } else {
        return CanonHostInfo::NEUTRAL;
      }
    }

    // Fill the buffer, if there's space remaining. This check allows us to
    // verify that all characters are numeric, even those that don't fit.
    if (dest_i < kMaxComponentLen) {
      buf[dest_i++] = input;
    }
  }

  if (may_be_broken_octal) {
    return CanonHostInfo::BROKEN;
  }

  buf[dest_i] = '\0';

  // Use the 64-bit strtoi so we get a big number (no hex, decimal, or octal
  // number can overflow a 64-bit number in <= 16 characters).
  uint64_t num = _strtoui64(buf, nullptr, BaseForType(base));

  // Check for 32-bit overflow.
  if (num > std::numeric_limits<uint32_t>::max()) {
    return CanonHostInfo::BROKEN;
  }

  // No overflow. Success!
  *number = static_cast<uint32_t>(num);
  return CanonHostInfo::IPV4;
}

// See declaration of IPv4AddressToNumber for documentation.
template <typename CHAR, typename UCHAR>
CanonHostInfo::Family DoIPv4AddressToNumber(const CHAR* spec,
                                            Component host,
                                            unsigned char address[4],
                                            int* num_ipv4_components) {
  // Ignore terminal dot, if present.
  if (host.is_nonempty() && spec[host.end() - 1] == '.') {
    --host.len;
  }

  // Do nothing if empty.
  if (host.is_empty()) {
    return CanonHostInfo::NEUTRAL;
  }

  // Read component values.  The first `existing_components` of them are
  // populated front to back, with the first one corresponding to the last
  // component, which allows for early exit if the last component isn't a
  // number.
  uint32_t component_values[4];
  int existing_components = 0;

  int current_component_end = host.end();
  int current_position = current_component_end;
  while (true) {
    // If this is not the first character of a component, go to the next
    // component.
    if (current_position != host.begin && spec[current_position - 1] != '.') {
      --current_position;
      continue;
    }

    CanonHostInfo::Family family = IPv4ComponentToNumber(
        spec,
        Component(current_position, current_component_end - current_position),
        &component_values[existing_components]);

    // If `family` is NEUTRAL and this is the last component, return NEUTRAL. If
    // `family` is NEUTRAL but not the last component, this is considered a
    // BROKEN IPv4 address, as opposed to a non-IPv4 hostname.
    if (family == CanonHostInfo::NEUTRAL && existing_components == 0) {
      return CanonHostInfo::NEUTRAL;
    }

    if (family != CanonHostInfo::IPV4) {
      return CanonHostInfo::BROKEN;
    }

    ++existing_components;

    // If this is the final component, nothing else to do.
    if (current_position == host.begin) {
      break;
    }

    // If there are more than 4 components, fail.
    if (existing_components == 4) {
      return CanonHostInfo::BROKEN;
    }

    current_component_end = current_position - 1;
    --current_position;
  }

  // Use `component_values` to fill out the 4-component IP address.

  // First, process all components but the last, while making sure each fits
  // within an 8-bit field.
  for (int i = existing_components - 1; i > 0; i--) {
    if (component_values[i] > std::numeric_limits<uint8_t>::max()) {
      return CanonHostInfo::BROKEN;
    }
    address[existing_components - i - 1] =
        static_cast<unsigned char>(component_values[i]);
  }

  uint32_t last_value = component_values[0];
  for (int i = 3; i >= existing_components - 1; i--) {
    address[i] = static_cast<unsigned char>(last_value);
    last_value >>= 8;
  }

  // If the last component has residual bits, report overflow.
  if (last_value != 0) {
    return CanonHostInfo::BROKEN;
  }

  // Tell the caller how many components we saw.
  *num_ipv4_components = existing_components;

  // Success!
  return CanonHostInfo::IPV4;
}

// Helper class that describes the main components of an IPv6 input string.
// See the following examples to understand how it breaks up an input string:
//
// [Example 1]: input = "[::aa:bb]"
//  ==> num_hex_components = 2
//  ==> hex_components[0] = Component(3,2) "aa"
//  ==> hex_components[1] = Component(6,2) "bb"
//  ==> index_of_contraction = 0
//  ==> ipv4_component = Component(0, -1)
//
// [Example 2]: input = "[1:2::3:4:5]"
//  ==> num_hex_components = 5
//  ==> hex_components[0] = Component(1,1) "1"
//  ==> hex_components[1] = Component(3,1) "2"
//  ==> hex_components[2] = Component(6,1) "3"
//  ==> hex_components[3] = Component(8,1) "4"
//  ==> hex_components[4] = Component(10,1) "5"
//  ==> index_of_contraction = 2
//  ==> ipv4_component = Component(0, -1)
//
// [Example 3]: input = "[::ffff:192.168.0.1]"
//  ==> num_hex_components = 1
//  ==> hex_components[0] = Component(3,4) "ffff"
//  ==> index_of_contraction = 0
//  ==> ipv4_component = Component(8, 11) "192.168.0.1"
//
// [Example 4]: input = "[1::]"
//  ==> num_hex_components = 1
//  ==> hex_components[0] = Component(1,1) "1"
//  ==> index_of_contraction = 1
//  ==> ipv4_component = Component(0, -1)
//
// [Example 5]: input = "[::192.168.0.1]"
//  ==> num_hex_components = 0
//  ==> index_of_contraction = 0
//  ==> ipv4_component = Component(8, 11) "192.168.0.1"
//
struct IPv6Parsed {
  // Zero-out the parse information.
  void reset() {
    num_hex_components = 0;
    index_of_contraction = -1;
    ipv4_component.reset();
  }

  // There can be up to 8 hex components (colon separated) in the literal.
  Component hex_components[8];

  // The count of hex components present. Ranges from [0,8].
  int num_hex_components;

  // The index of the hex component that the "::" contraction precedes, or
  // -1 if there is no contraction.
  int index_of_contraction;

  // The range of characters which are an IPv4 literal.
  Component ipv4_component;
};

// Parse the IPv6 input string. If parsing succeeded returns true and fills
// |parsed| with the information. If parsing failed (because the input is
// invalid) returns false.
template <typename CHAR, typename UCHAR>
bool DoParseIPv6(const CHAR* spec, const Component& host, IPv6Parsed* parsed) {
  // Zero-out the info.
  parsed->reset();

  if (host.is_empty()) {
    return false;
  }

  // The index for start and end of address range (no brackets).
  int begin = host.begin;
  int end = host.end();

  int cur_component_begin = begin;  // Start of the current component.

  // Scan through the input, searching for hex components, "::" contractions,
  // and IPv4 components.
  for (int i = begin; /* i <= end */; i++) {
    bool is_colon = spec[i] == ':';
    bool is_contraction = is_colon && i < end - 1 && spec[i + 1] == ':';

    // We reached the end of the current component if we encounter a colon
    // (separator between hex components, or start of a contraction), or end of
    // input.
    if (is_colon || i == end) {
      int component_len = i - cur_component_begin;

      // A component should not have more than 4 hex digits.
      if (component_len > 4) {
        return false;
      }

      // Don't allow empty components.
      if (component_len == 0) {
        // The exception is when contractions appear at beginning of the
        // input or at the end of the input.
        if (!((is_contraction && i == begin) ||
              (i == end &&
               parsed->index_of_contraction == parsed->num_hex_components))) {
          return false;
        }
      }

      // Add the hex component we just found to running list.
      if (component_len > 0) {
        // Can't have more than 8 components!
        if (parsed->num_hex_components >= 8) {
          return false;
        }

        parsed->hex_components[parsed->num_hex_components++] =
            Component(cur_component_begin, component_len);
      }
    }

    if (i == end) {
      break;  // Reached the end of the input, DONE.
    }

    // We found a "::" contraction.
    if (is_contraction) {
      // There can be at most one contraction in the literal.
      if (parsed->index_of_contraction != -1) {
        return false;
      }
      parsed->index_of_contraction = parsed->num_hex_components;
      ++i;  // Consume the colon we peeked.
    }

    if (is_colon) {
      // Colons are separators between components, keep track of where the
      // current component started (after this colon).
      cur_component_begin = i + 1;
    } else {
      if (static_cast<UCHAR>(spec[i]) >= 0x80) {
        return false;  // Not ASCII.
      }

      if (!IsHexChar(static_cast<unsigned char>(spec[i]))) {
        // Regular components are hex numbers. It is also possible for
        // a component to be an IPv4 address in dotted form.
        if (IsIPv4Char(static_cast<unsigned char>(spec[i]))) {
          // Since IPv4 address can only appear at the end, assume the rest
          // of the string is an IPv4 address. (We will parse this separately
          // later).
          parsed->ipv4_component =
              Component(cur_component_begin, end - cur_component_begin);
          break;
        } else {
          // The character was neither a hex digit, nor an IPv4 character.
          return false;
        }
      }
    }
  }

  return true;
}

// Verifies the parsed IPv6 information, checking that the various components
// add up to the right number of bits (hex components are 16 bits, while
// embedded IPv4 formats are 32 bits, and contractions are placeholders for
// 16 or more bits). Returns true if sizes match up, false otherwise. On
// success writes the length of the contraction (if any) to
// |out_num_bytes_of_contraction|.
bool CheckIPv6ComponentsSize(const IPv6Parsed& parsed,
                             int* out_num_bytes_of_contraction) {
  // Each group of four hex digits contributes 16 bits.
  int num_bytes_without_contraction = parsed.num_hex_components * 2;

  // If an IPv4 address was embedded at the end, it contributes 32 bits.
  if (parsed.ipv4_component.is_valid()) {
    num_bytes_without_contraction += 4;
  }

  // If there was a "::" contraction, its size is going to be:
  // MAX([16bits], [128bits] - num_bytes_without_contraction).
  int num_bytes_of_contraction = 0;
  if (parsed.index_of_contraction != -1) {
    num_bytes_of_contraction = 16 - num_bytes_without_contraction;
    if (num_bytes_of_contraction < 2) {
      num_bytes_of_contraction = 2;
    }
  }

  // Check that the numbers add up.
  if (num_bytes_without_contraction + num_bytes_of_contraction != 16) {
    return false;
  }

  *out_num_bytes_of_contraction = num_bytes_of_contraction;
  return true;
}

// Converts a hex component into a number. This cannot fail since the caller has
// already verified that each character in the string was a hex digit, and
// that there were no more than 4 characters.
template <typename CHAR>
uint16_t IPv6HexComponentToNumber(const CHAR* spec,
                                  const Component& component) {
  DCHECK(component.len <= 4);

  // Copy the hex string into a C-string.
  char buf[5];
  for (int i = 0; i < component.len; ++i) {
    buf[i] = static_cast<char>(spec[component.begin + i]);
  }
  buf[component.len] = '\0';

  // Convert it to a number (overflow is not possible, since with 4 hex
  // characters we can at most have a 16 bit number).
  return static_cast<uint16_t>(_strtoui64(buf, nullptr, 16));
}

// Converts an IPv6 address to a 128-bit number (network byte order), returning
// true on success. False means that the input was not a valid IPv6 address.
template <typename CHAR, typename UCHAR>
bool DoIPv6AddressToNumber(const CHAR* spec,
                           const Component& host,
                           unsigned char address[16]) {
  // Make sure the component is bounded by '[' and ']'.
  int end = host.end();
  if (host.is_empty() || spec[host.begin] != '[' || spec[end - 1] != ']') {
    return false;
  }

  // Exclude the square brackets.
  Component ipv6_comp(host.begin + 1, host.len - 2);

  // Parse the IPv6 address -- identify where all the colon separated hex
  // components are, the "::" contraction, and the embedded IPv4 address.
  IPv6Parsed ipv6_parsed;
  if (!DoParseIPv6<CHAR, UCHAR>(spec, ipv6_comp, &ipv6_parsed)) {
    return false;
  }

  // Do some basic size checks to make sure that the address doesn't
  // specify more than 128 bits or fewer than 128 bits. This also resolves
  // how may zero bytes the "::" contraction represents.
  int num_bytes_of_contraction;
  if (!CheckIPv6ComponentsSize(ipv6_parsed, &num_bytes_of_contraction)) {
    return false;
  }

  int cur_index_in_address = 0;

  // Loop through each hex components, and contraction in order.
  for (int i = 0; i <= ipv6_parsed.num_hex_components; ++i) {
    // Append the contraction if it appears before this component.
    if (i == ipv6_parsed.index_of_contraction) {
      for (int j = 0; j < num_bytes_of_contraction; ++j) {
        address[cur_index_in_address++] = 0;
      }
    }
    // Append the hex component's value.
    if (i != ipv6_parsed.num_hex_components) {
      // Get the 16-bit value for this hex component.
      uint16_t number =
          IPv6HexComponentToNumber<CHAR>(spec, ipv6_parsed.hex_components[i]);
      // Append to |address|, in network byte order.
      address[cur_index_in_address++] = (number & 0xFF00) >> 8;
      address[cur_index_in_address++] = (number & 0x00FF);
    }
  }

  // If there was an IPv4 section, convert it into a 32-bit number and append
  // it to |address|.
  if (ipv6_parsed.ipv4_component.is_valid()) {
    // Append the 32-bit number to |address|.
    int ignored_num_ipv4_components;
    if (CanonHostInfo::IPV4 !=
        IPv4AddressToNumber(spec, ipv6_parsed.ipv4_component,
                            &address[cur_index_in_address],
                            &ignored_num_ipv4_components)) {
      return false;
    }
  }

  return true;
}

// Searches for the longest sequence of zeros in |address|, and writes the
// range into |contraction_range|. The run of zeros must be at least 16 bits,
// and if there is a tie the first is chosen.
void ChooseIPv6ContractionRange(const unsigned char address[16],
                                Component* contraction_range) {
  // The longest run of zeros in |address| seen so far.
  Component max_range;

  // The current run of zeros in |address| being iterated over.
  Component cur_range;

  for (int i = 0; i < 16; i += 2) {
    // Test for 16 bits worth of zero.
    bool is_zero = (address[i] == 0 && address[i + 1] == 0);

    if (is_zero) {
      // Add the zero to the current range (or start a new one).
      if (!cur_range.is_valid()) {
        cur_range = Component(i, 0);
      }
      cur_range.len += 2;
    }

    if (!is_zero || i == 14) {
      // Just completed a run of zeros. If the run is greater than 16 bits,
      // it is a candidate for the contraction.
      if (cur_range.len > 2 && cur_range.len > max_range.len) {
        max_range = cur_range;
      }
      cur_range.reset();
    }
  }
  *contraction_range = max_range;
}

}  // namespace

void AppendIPv4Address(const unsigned char address[4], CanonOutput* output) {
  for (int i = 0; i < 4; i++) {
    char str[16];
    _itoa_s(address[i], str, 10);

    for (int ch = 0; str[ch] != 0; ch++) {
      output->push_back(str[ch]);
    }

    if (i != 3) {
      output->push_back('.');
    }
  }
}

void AppendIPv6Address(const unsigned char address[16], CanonOutput* output) {
  // We will output the address according to the rules in:
  // http://tools.ietf.org/html/draft-kawamura-ipv6-text-representation-01#section-4

  // Start by finding where to place the "::" contraction (if any).
  Component contraction_range;
  ChooseIPv6ContractionRange(address, &contraction_range);

  for (int i = 0; i <= 14;) {
    // We check 2 bytes at a time, from bytes (0, 1) to (14, 15), inclusive.
    DCHECK(i % 2 == 0);
    if (i == contraction_range.begin && contraction_range.len > 0) {
      // Jump over the contraction.
      if (i == 0) {
        output->push_back(':');
      }
      output->push_back(':');
      i = contraction_range.end();
    } else {
      // Consume the next 16 bits from |address|.
      int x = address[i] << 8 | address[i + 1];

      i += 2;

      // Stringify the 16 bit number (at most requires 4 hex digits).
      char str[5];
      _itoa_s(x, str, 16);
      for (int ch = 0; str[ch] != 0; ++ch) {
        output->push_back(str[ch]);
      }

      // Put a colon after each number, except the last.
      if (i < 16) {
        output->push_back(':');
      }
    }
  }
}

CanonHostInfo::Family IPv4AddressToNumber(const char* spec,
                                          const Component& host,
                                          unsigned char address[4],
                                          int* num_ipv4_components) {
  return DoIPv4AddressToNumber<char, unsigned char>(spec, host, address,
                                                    num_ipv4_components);
}

CanonHostInfo::Family IPv4AddressToNumber(const char16_t* spec,
                                          const Component& host,
                                          unsigned char address[4],
                                          int* num_ipv4_components) {
  return DoIPv4AddressToNumber<char16_t, char16_t>(spec, host, address,
                                                   num_ipv4_components);
}

bool IPv6AddressToNumber(const char* spec,
                         const Component& host,
                         unsigned char address[16]) {
  return DoIPv6AddressToNumber<char, unsigned char>(spec, host, address);
}

bool IPv6AddressToNumber(const char16_t* spec,
                         const Component& host,
                         unsigned char address[16]) {
  return DoIPv6AddressToNumber<char16_t, char16_t>(spec, host, address);
}

}  // namespace redaction_internal
