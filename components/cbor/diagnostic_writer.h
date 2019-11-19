// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_DIAGNOSTIC_WRITER_H_
#define COMPONENTS_CBOR_DIAGNOSTIC_WRITER_H_

#include <string>

#include "components/cbor/cbor_export.h"

namespace cbor {

class Value;

class CBOR_EXPORT DiagnosticWriter {
 public:
  // Write converts the given CBOR value to a compact string, following the
  // "Diagnostic Notation" format for CBOR
  // (https://tools.ietf.org/html/rfc7049#section-6). |rough_max_output_bytes|
  // provides a loose upper bound on the size of the result and the result may
  // be truncated if it exceeds this size.
  static std::string Write(const Value& node,
                           size_t rough_max_output_bytes = 4096);
};

}  // namespace cbor

#endif  // COMPONENTS_CBOR_DIAGNOSTIC_WRITER_H_
