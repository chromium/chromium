// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/variant.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "components/dbus/utils/signature.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace dbus_utils {

Variant::Variant() : state_(std::monostate{}) {}

Variant::Variant(Variant&& other) noexcept
    : signature_(std::move(other.signature_)), state_(std::move(other.state_)) {
  other.state_.emplace<std::monostate>();
}

Variant& Variant::operator=(Variant&& other) noexcept {
  if (this != &other) {
    signature_ = std::move(other.signature_);
    state_ = std::move(other.state_);
    other.state_.emplace<std::monostate>();
  }
  return *this;
}

Variant::~Variant() = default;

bool Variant::operator==(const Variant& other) const {
  return std::visit(
      absl::Overload{
          [](const base::ScopedFD& l, const base::ScopedFD& r) -> bool {
            return l.get() == r.get();
          },
          [](const NestedVariant& l, const NestedVariant& r) -> bool {
            CHECK(l);
            CHECK(r);
            return *l == *r;
          },
          []<typename T>(const T& l, const T& r) -> bool {
            // All other types can be compared directly.
            return l == r;
          },
          [](const auto& l, const auto& r) -> bool {
            // Different types, cannot be equal.
            return false;
          },
      },
      state_, other.state_);
}

void Variant::Write(dbus::MessageWriter& writer) const {
  dbus::MessageWriter variant_writer(nullptr);
  writer.OpenVariant(signature_, &variant_writer);
  WriteContent(variant_writer);
  writer.CloseContainer(&variant_writer);
}

void Variant::WriteContent(dbus::MessageWriter& writer) const {
  std::visit(absl::Overload{
                 [&](const std::monostate&) { NOTREACHED(); },
                 [&](const bool& v) { writer.AppendBool(v); },
                 [&](const uint8_t& v) { writer.AppendByte(v); },
                 [&](const int16_t& v) { writer.AppendInt16(v); },
                 [&](const uint16_t& v) { writer.AppendUint16(v); },
                 [&](const int32_t& v) { writer.AppendInt32(v); },
                 [&](const uint32_t& v) { writer.AppendUint32(v); },
                 [&](const int64_t& v) { writer.AppendInt64(v); },
                 [&](const uint64_t& v) { writer.AppendUint64(v); },
                 [&](const double& v) { writer.AppendDouble(v); },
                 [&](const std::string& v) { writer.AppendString(v); },
                 [&](const dbus::ObjectPath& v) { writer.AppendObjectPath(v); },
                 [&](const base::ScopedFD& v) {
                   writer.AppendFileDescriptor(v.get());
                 },
                 [&](const Dictionary& v) {
                   std::string member_sig = signature_.substr(1);
                   dbus::MessageWriter array_writer(nullptr);
                   writer.OpenArray(member_sig.c_str(), &array_writer);
                   for (const auto& pair : v) {
                     dbus::MessageWriter entry_writer(nullptr);
                     array_writer.OpenDictEntry(&entry_writer);
                     pair.first.WriteContent(entry_writer);
                     pair.second.WriteContent(entry_writer);
                     array_writer.CloseContainer(&entry_writer);
                   }
                   writer.CloseContainer(&array_writer);
                 },
                 [&](const Sequence& v) {
                   CHECK(!signature_.empty());
                   if (signature_[0] == DBUS_STRUCT_BEGIN_CHAR) {
                     dbus::MessageWriter struct_writer(nullptr);
                     writer.OpenStruct(&struct_writer);
                     for (const auto& member : v) {
                       member.WriteContent(struct_writer);
                     }
                     writer.CloseContainer(&struct_writer);
                   } else {
                     CHECK_EQ(signature_[0], DBUS_TYPE_ARRAY);
                     dbus::MessageWriter array_writer(nullptr);
                     writer.OpenArray(signature_.substr(1), &array_writer);
                     for (const auto& element : v) {
                       element.WriteContent(array_writer);
                     }
                     writer.CloseContainer(&array_writer);
                   }
                 },
                 [&](const NestedVariant& v) { v->Write(writer); }},
             state_);
}

bool Variant::Read(dbus::MessageReader& reader) {
  dbus::MessageReader variant_reader(nullptr);
  if (!reader.PopVariant(&variant_reader)) {
    return false;
  }

  std::string signature = variant_reader.GetDataSignature();
  if (!ReadContent(variant_reader) || variant_reader.HasMoreData()) {
    return false;
  }
  signature_ = std::move(signature);
  return true;
}

bool Variant::ReadContent(dbus::MessageReader& reader) {
  auto read_primitive =
      [&]<typename T>(bool (dbus::MessageReader::*pop)(T*)) -> bool {
    T v;
    if (!(reader.*pop)(&v)) {
      return false;
    }
    state_.emplace<T>(std::move(v));
    return true;
  };

  switch (reader.GetDataType()) {
    case dbus::Message::DataType::BOOL:
      return read_primitive(&dbus::MessageReader::PopBool);
    case dbus::Message::DataType::BYTE:
      return read_primitive(&dbus::MessageReader::PopByte);
    case dbus::Message::DataType::INT16:
      return read_primitive(&dbus::MessageReader::PopInt16);
    case dbus::Message::DataType::UINT16:
      return read_primitive(&dbus::MessageReader::PopUint16);
    case dbus::Message::DataType::INT32:
      return read_primitive(&dbus::MessageReader::PopInt32);
    case dbus::Message::DataType::UINT32:
      return read_primitive(&dbus::MessageReader::PopUint32);
    case dbus::Message::DataType::INT64:
      return read_primitive(&dbus::MessageReader::PopInt64);
    case dbus::Message::DataType::UINT64:
      return read_primitive(&dbus::MessageReader::PopUint64);
    case dbus::Message::DataType::DOUBLE:
      return read_primitive(&dbus::MessageReader::PopDouble);
    case dbus::Message::DataType::STRING:
      return read_primitive(&dbus::MessageReader::PopString);
    case dbus::Message::DataType::OBJECT_PATH:
      return read_primitive(&dbus::MessageReader::PopObjectPath);
    case dbus::Message::DataType::UNIX_FD:
      return read_primitive(&dbus::MessageReader::PopFileDescriptor);
    case dbus::Message::DataType::VARIANT: {
      auto nested_variant = std::make_unique<Variant>();
      if (!nested_variant->Read(reader)) {
        return false;
      }
      state_.emplace<NestedVariant>(std::move(nested_variant));
      return true;
    }
    case dbus::Message::DataType::ARRAY: {
      std::string reader_signature = reader.GetDataSignature();
      if (reader_signature.length() > 1 &&
          reader_signature[1] == DBUS_DICT_ENTRY_BEGIN_CHAR) {
        // Dictionary
        dbus::MessageReader array_reader(nullptr);
        if (!reader.PopArray(&array_reader)) {
          return false;
        }
        Dictionary dict_data;
        while (array_reader.HasMoreData()) {
          dbus::MessageReader entry_reader(nullptr);
          if (!array_reader.PopDictEntry(&entry_reader)) {
            return false;
          }
          Variant key, value;
          std::string key_signature = entry_reader.GetDataSignature();
          if (!key.ReadContent(entry_reader) || !entry_reader.HasMoreData()) {
            return false;
          }
          key.signature_ = std::move(key_signature);
          std::string value_signature = entry_reader.GetDataSignature();
          if (!value.ReadContent(entry_reader) || entry_reader.HasMoreData()) {
            return false;
          }
          value.signature_ = std::move(value_signature);
          dict_data.emplace_back(std::move(key), std::move(value));
        }
        state_.emplace<Dictionary>(std::move(dict_data));
        return true;
      } else {
        // Array
        dbus::MessageReader array_reader(nullptr);
        if (!reader.PopArray(&array_reader)) {
          return false;
        }
        Sequence seq_data;
        while (array_reader.HasMoreData()) {
          Variant element;
          std::string element_signature = array_reader.GetDataSignature();
          if (!element.ReadContent(array_reader)) {
            return false;
          }
          element.signature_ = std::move(element_signature);
          seq_data.push_back(std::move(element));
        }
        state_.emplace<Sequence>(std::move(seq_data));
        return true;
      }
    }
    case dbus::Message::DataType::STRUCT: {
      dbus::MessageReader struct_reader(nullptr);
      if (!reader.PopStruct(&struct_reader)) {
        return false;
      }
      Sequence seq_data;
      while (struct_reader.HasMoreData()) {
        Variant member;
        std::string member_signature = struct_reader.GetDataSignature();
        if (!member.ReadContent(struct_reader)) {
          return false;
        }
        member.signature_ = std::move(member_signature);
        seq_data.push_back(std::move(member));
      }
      state_.emplace<Sequence>(std::move(seq_data));
      return true;
    }
    case dbus::Message::DataType::DICT_ENTRY:
    case dbus::Message::DataType::INVALID_DATA:
      return false;
  }
  return false;
}

}  // namespace dbus_utils
