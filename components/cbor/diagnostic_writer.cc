// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/diagnostic_writer.h"

#include <string>

#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/constants.h"
#include "components/cbor/values.h"

using base::ClampAdd;
using base::ClampMul;

namespace cbor {

static bool AppendHex(const std::vector<uint8_t> bytes,
                      char type_char,
                      size_t rough_max_output_bytes,
                      std::string* s) {
  s->push_back(type_char);
  s->push_back('\'');

  if (ClampAdd(s->size(), ClampMul(2u, bytes.size())) >
      rough_max_output_bytes) {
    return false;
  }
  s->append(base::HexEncode(bytes.data(), bytes.size()));

  s->push_back('\'');
  return true;
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
          NOTREACHED();
          break;
      }
      break;

    case Value::Type::NONE:
      s->append("none");
      break;

    case Value::Type::TAG:
      NOTREACHED();
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
