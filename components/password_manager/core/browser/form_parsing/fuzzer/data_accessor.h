// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_DATA_ACCESSOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_DATA_ACCESSOR_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace password_manager {

// DataAccessor is an encapsulation over the input string delivered by the
// fuzzer framework. It pads the input string with zeroes after its end and
// deliveres data based on the consumed string. More details in the design doc
// linked from https://crbug.com/827945#c2.
class DataAccessor {
 public:
  // Wraps the input string of length |size| at address |data|. Does not own the
  // input string. It sets the "reading head" to the start of the string and
  // advances it with each Consume* operation to avoid generating data from the
  // same part of input twice.
  DataAccessor(const uint8_t* data, size_t size);

  DataAccessor(const DataAccessor&) = delete;
  DataAccessor& operator=(const DataAccessor&) = delete;

  ~DataAccessor();

  // Return the next bit and advance the "reading head" by one bit.
  bool ConsumeBit();

  // Return the number stored on the next |bit_length| bits and advance the
  // "reading head" by |bit_length| bits.
  size_t ConsumeNumber(size_t bit_length);

  // Advance the "reading head" to the next whole-byte boundary, if needed, then
  // return the string stored in the next |length| characters, advancing the
  // "reading head" to point past the read data. A "character" means byte for
  // std::string and two bytes for std::u16string. At most 256 bytes can be
  // consumed at once, hence |length| is restricted as noted below.
  std::string ConsumeString(size_t length);       // |length| <= 256
  std::u16string ConsumeString16(size_t length);  // |length| <= 128

 private:
  // Helper for |ConsumeString*|. It combines the |data_| and padding, if
  // needed, into |string_buffer|, to provide |length| bytes for creating a new
  // string. It also updates |data_|, |bits_consumed_| and |size_| accordingly.
  void ConsumeBytesToBuffer(size_t length, uint8_t* string_buffer);

  // The remaining portion of the input string (without padding) starts at
  // |data_| without the least significant |bits_consumed_| bits, and lasts
  // until |data_ + size_|, exclusively. If |size_| is 0, then there is not a
  // single bit left and all available is just the 0-padding.
  // Invariants:
  // * |data_| is not null as long as |size_| > 0
  // * |bits_consumed_| < 8
  // * if |size_| == 0 then |bits_consumed_| == 0
  const uint8_t* data_;
  size_t bits_consumed_;
  size_t size_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_DATA_ACCESSOR_H_
