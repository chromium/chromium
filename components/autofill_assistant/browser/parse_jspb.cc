// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/parse_jspb.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "base/bit_cast.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace autofill_assistant {
namespace {

// Proto buffer wire types needed in this file.
//
// This avoids including
// third_party/protobuf/src/google/protobuf/wire_format_lite.h since it is
// documented as being internal.
enum WireType {
  WIRETYPE_VARINT = 0,
  WIRETYPE_LENGTH_DELIMITED = 2,
  WIRETYPE_FIXED32 = 5,
};

// Writes a field tag, containing both tag and wire type.
//
// This avoids including
// third_party/protobuf/src/google/protobuf/wire_format_lite.h since it is
// documented as being internal.
inline void WriteTag(uint32_t tag,
                     WireType type,
                     google::protobuf::io::CodedOutputStream* out) {
  out->WriteTag((tag << 3) | type);
}

// Returns true if |list| contains a message.
//
// Support is limited to messages with a jspb id starting with the expected
// prefix.
bool IsJspbMessage(const std::string& jspb_id_prefix,
                   const base::Value& value,
                   std::string* error_message) {
  if (!value.is_list()) {
    return false;
  }
  const base::Value::List& list = value.GetList();
  if (list.empty() || !list.front().is_string() ||
      !base::StartsWith(list.front().GetString(), jspb_id_prefix)) {
    if (error_message != nullptr) {
      *error_message =
          base::StrCat({"Message id must start with '", jspb_id_prefix, "'"});
    }
    return false;
  }
  return true;
}

bool ParseJspbToString(const std::string& jspb_id_prefix,
                       const base::Value::List& list,
                       std::string* bytes,
                       std::string* error_message);

// Assign |field_tag| to |value| in the output stream |out|.
bool AppendFieldValue(const std::string& jspb_id_prefix,
                      uint32_t field_tag,
                      const base::Value& value,
                      google::protobuf::io::CodedOutputStream* out,
                      std::string* error_message) {
  switch (value.type()) {
    case base::Value::Type::BOOLEAN:
      WriteTag(field_tag, WIRETYPE_VARINT, out);
      out->WriteVarint32(value.GetBool() ? 1 : 0);
      break;

    case base::Value::Type::INTEGER:
      WriteTag(field_tag, WIRETYPE_VARINT, out);
      out->WriteVarint64(value.GetInt());
      break;

    case base::Value::Type::DOUBLE:
      // Encode these as floats (in a fixed32)
      WriteTag(field_tag, WIRETYPE_FIXED32, out);
      out->WriteLittleEndian32(
          base::bit_cast<uint32_t>(static_cast<float>(value.GetDouble())));
      break;

    case base::Value::Type::STRING: {
      const std::string& string = value.GetString();
      WriteTag(field_tag, WIRETYPE_LENGTH_DELIMITED, out);
      out->WriteVarint32(string.size());
      out->WriteString(string);
      break;
    }

    case base::Value::Type::LIST:
      if (IsJspbMessage(jspb_id_prefix, value, /* error_message= */ nullptr)) {
        // A proto message
        std::string bytes;
        if (!ParseJspbToString(jspb_id_prefix, value.GetList(), &bytes,
                               error_message)) {
          return false;
        }
        WriteTag(field_tag, WIRETYPE_LENGTH_DELIMITED, out);
        out->WriteVarint32(bytes.size());
        out->WriteString(bytes);
        break;
      }
      // A repeated field.
      for (const base::Value& element : value.GetList()) {
        AppendFieldValue(jspb_id_prefix, field_tag, element, out,
                         error_message);
      }
      break;

    default:
      if (error_message != nullptr) {
        *error_message = base::StrCat({"Unexpected value type for field ",
                                       base::NumberToString(field_tag), ": ",
                                       base::Value::GetTypeName(value.type())});
      }
      return false;
  }
  return true;
}

// Parses a message from |list| and puts the result into |bytes|.
bool ParseJspbToString(const std::string& jspb_id_prefix,
                       const base::Value::List& list,
                       std::string* bytes,
                       std::string* error_message) {
  google::protobuf::io::StringOutputStream string_output(bytes);
  google::protobuf::io::CodedOutputStream coded_output(&string_output);

  // IsJspbMessage has found a message_id in list[0]; the rest of the list
  // contains fields.
  for (size_t i = 1; i < list.size(); i++) {
    const base::Value& array_value = list[i];
    if (array_value.is_none()) {
      // field has no value (null)
      continue;
    }
    if (array_value.is_dict()) {
      // The remaining fields are stored in a dict, with keys as tag numbers.
      // Example: ["message_id", null, null, 1, {"112": 9, ...}]
      for (const auto [field_key, field_value] : array_value.DictItems()) {
        uint32_t tag = 0;
        if (!base::StringToUint(field_key, &tag)) {
          if (error_message != nullptr) {
            *error_message = base::StrCat(
                {"Dictionary keys should be stringified integers, not '",
                 field_key, "'"});
          }
          return false;
        }
        if (!AppendFieldValue(jspb_id_prefix, tag, field_value, &coded_output,
                              error_message)) {
          return false;
        }
      }
      return true;
    }
    // Fields are stored in the array, at a position corresponding to their tag
    // number.
    uint32_t field_tag = static_cast<uint32_t>(i);
    if (!AppendFieldValue(jspb_id_prefix, field_tag, array_value, &coded_output,
                          error_message)) {
      return false;
    }
  }
  return true;
}

}  // namespace

absl::optional<std::string> ParseJspb(const std::string& jspb_id_prefix,
                                      const base::Value& message,
                                      std::string* error_message) {
  if (!IsJspbMessage(jspb_id_prefix, message, error_message)) {
    return absl::nullopt;
  }
  std::string bytes;
  if (!ParseJspbToString(jspb_id_prefix, message.GetList(), &bytes,
                         error_message)) {
    return absl::nullopt;
  }
  return bytes;
}

}  // namespace autofill_assistant
