// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/diagnostic_writer.h"

#include <string>

#include "base/json/string_escape.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/cbor/constants.h"
#include "components/cbor/values.h"

using base::ClampAdd;
using base::ClampMul;

namespace cbor {

static bool AppendHex(const std::vector<uint8_t> bytes,
                      char type_char,
                      size_t rough_max_output_bytes,
                      std::string* s) {
  if (s->size() > rough_max_output_bytes) {
    return false;
  }

  const size_t hex_size = ClampMul(2u, bytes.size());
  // If the hex string would be longer than 87.5% of the total output space, or
  // if it would cause the current string to be too long, replace it with an
  // indication of its length. (87.5% was chosen because it's easy to
  // calculate and is reasonable.)
  if (hex_size > rough_max_output_bytes - (rough_max_output_bytes >> 3) ||
      ClampAdd(s->size(), hex_size) >= rough_max_output_bytes) {
    s->append(base::StringPrintf("(%zu bytes)", bytes.size()));
  } else {
    s->push_back(type_char);
    s->push_back('\'');
    s->append(base::HexEncode(bytes));
    s->push_back('\'');
  }

  return s->size() < rough_max_output_bytes;
}

static bool Serialize(const Value& node,
                      size_t rough_max_output_bytes,
                      std::string* s) {
  switch (node.type()) {
    case Value::Type::UNSIGNED:
      s->append(base::NumberToString(node.GetUnsigned()));
      break;

    case Value::Type::NEGATIVE:
      s->append(base::NumberToString(node.GetNegative()));
      break;

    case Value::Type::INVALID_UTF8:
      if (!AppendHex(node.GetInvalidUTF8(), 's', rough_max_output_bytes, s)) {
        return false;
      }
      break;

    case Value::Type::BYTE_STRING:
      if (!AppendHex(node.GetBytestring(), 'h', rough_max_output_bytes, s)) {
        return false;
      }
      break;

    case Value::Type::STRING: {
      std::string quoted_and_escaped;
      base::EscapeJSONString(node.GetString(), /*put_in_quotes=*/true,
                             &quoted_and_escaped);
      if (ClampAdd(s->size(), quoted_and_escaped.size()) >
          rough_max_output_bytes) {
        return false;
      }
      s->append(quoted_and_escaped);
      break;
    }

    case Value::Type::ARRAY: {
      s->push_back('[');

      const Value::ArrayValue& nodes = node.GetArray();
      bool first = true;
      for (const auto& subnode : nodes) {
        if (!first) {
          s->append(", ");
        }
        if (!Serialize(subnode, rough_max_output_bytes, s) ||
            s->size() > rough_max_output_bytes) {
          return false;
        }
        first = false;
      }

      s->push_back(']');
      break;
    }

    case Value::Type::MAP: {
      s->push_back('{');

      const Value::MapValue& nodes = node.GetMap();
      bool first = true;
      for (const auto& pair : nodes) {
        if (!first) {
          s->append(", ");
        }
        if (!Serialize(pair.first, rough_max_output_bytes, s)) {
          return false;
        }
        s->append(": ");
        if (!Serialize(pair.second, rough_max_output_bytes, s) ||
            s->size() > rough_max_output_bytes) {
          return false;
        }
        first = false;
      }

      s->push_back('}');
      break;
    }

    case Value::Type::SIMPLE_VALUE:
      switch (node.GetSimpleValue()) {
        case Value::SimpleValue::FALSE_VALUE:
          s->append("false");
          break;
        case Value::SimpleValue::TRUE_VALUE:
          s->append("true");
          break;
        case Value::SimpleValue::NULL_VALUE:
          s->append("null");
          break;
        case Value::SimpleValue::UNDEFINED:
          s->append("undefined");
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      break;
    case Value::Type::FLOAT_VALUE:
      s->append(base::NumberToString(node.GetDouble()));
      break;
    case Value::Type::NONE:
      s->append("none");
      break;

    case Value::Type::TAG:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return true;
}

// static
std::string DiagnosticWriter::Write(const Value& node,
                                    size_t rough_max_output_bytes) {
  std::string ret;
  Serialize(node, rough_max_output_bytes, &ret);
  return ret;
}

}  // namespace cbor
