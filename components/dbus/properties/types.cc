// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/properties/types.h"

#include "dbus/message.h"
#include "dbus/object_path.h"

DbusType::~DbusType() = default;

bool DbusType::operator==(const DbusType& other) const {
  if (GetSignatureDynamic() != other.GetSignatureDynamic())
    return false;
  return IsEqual(other);
}
bool DbusType::operator!=(const DbusType& other) const {
  return !(*this == other);
}

DbusBoolean::DbusBoolean(bool value) : value_(value) {}
DbusBoolean::DbusBoolean(DbusBoolean&& other) = default;
DbusBoolean::~DbusBoolean() = default;

void DbusBoolean::Write(dbus::MessageWriter* writer) const {
  writer->AppendBool(value_);
}

// static
std::string DbusBoolean::GetSignature() {
  return "b";
}

DbusInt32::DbusInt32(int32_t value) : value_(value) {}
DbusInt32::DbusInt32(DbusInt32&& other) = default;
DbusInt32::~DbusInt32() = default;

void DbusInt32::Write(dbus::MessageWriter* writer) const {
  writer->AppendInt32(value_);
}

// static
std::string DbusInt32::GetSignature() {
  return "i";
}

DbusUint32::DbusUint32(uint32_t value) : value_(value) {}
DbusUint32::DbusUint32(DbusUint32&& other) = default;
DbusUint32::~DbusUint32() = default;

void DbusUint32::Write(dbus::MessageWriter* writer) const {
  writer->AppendUint32(value_);
}

// static
std::string DbusUint32::GetSignature() {
  return "u";
}

DbusInt64::DbusInt64(int64_t value) : value_(value) {}
DbusInt64::DbusInt64(DbusInt64&& other) = default;
DbusInt64::~DbusInt64() = default;

void DbusInt64::Write(dbus::MessageWriter* writer) const {
  writer->AppendInt64(value_);
}

// static
std::string DbusInt64::GetSignature() {
  return "x";
}

DbusDouble::DbusDouble(double value) : value_(value) {}
DbusDouble::DbusDouble(DbusDouble&& other) = default;
DbusDouble::~DbusDouble() = default;

void DbusDouble::Write(dbus::MessageWriter* writer) const {
  writer->AppendDouble(value_);
}

// static
std::string DbusDouble::GetSignature() {
  return "d";
}

DbusString::DbusString(const std::string& value) : value_(value) {}
DbusString::DbusString(DbusString&& other) = default;
DbusString::~DbusString() = default;

void DbusString::Write(dbus::MessageWriter* writer) const {
  writer->AppendString(value_);
}

// static
std::string DbusString::GetSignature() {
  return "s";
}

DbusObjectPath::DbusObjectPath(const dbus::ObjectPath& value) : value_(value) {}
DbusObjectPath::DbusObjectPath(DbusObjectPath&& other) = default;
DbusObjectPath::~DbusObjectPath() = default;

void DbusObjectPath::Write(dbus::MessageWriter* writer) const {
  writer->AppendObjectPath(value_);
}

// static
std::string DbusObjectPath::GetSignature() {
  return "o";
}

DbusVariant::DbusVariant() = default;
DbusVariant::DbusVariant(std::unique_ptr<DbusType> value)
    : value_(std::move(value)) {}
DbusVariant::DbusVariant(DbusVariant&& other) = default;
DbusVariant::~DbusVariant() = default;
DbusVariant& DbusVariant::operator=(DbusVariant&& other) = default;

DbusVariant::operator bool() const {
  return !!value_;
}

bool DbusVariant::IsEqual(const DbusType& other_type) const {
  const DbusVariant* other = static_cast<const DbusVariant*>(&other_type);
  return *value_ == *other->value_;
}

void DbusVariant::Write(dbus::MessageWriter* writer) const {
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant(value_->GetSignatureDynamic(), &variant_writer);
  value_->Write(&variant_writer);
  writer->CloseContainer(&variant_writer);
}

// static
std::string DbusVariant::GetSignature() {
  return "v";
}

DbusByteArray::DbusByteArray() = default;
DbusByteArray::DbusByteArray(scoped_refptr<base::RefCountedMemory> value)
    : value_(value) {}
DbusByteArray::DbusByteArray(DbusByteArray&& other) = default;
DbusByteArray::~DbusByteArray() = default;

bool DbusByteArray::IsEqual(const DbusType& other_type) const {
  const DbusByteArray* other = static_cast<const DbusByteArray*>(&other_type);
  return value_->size() == other->value_->size() &&
         !memcmp(value_->front(), other->value_->front(), value_->size());
}

void DbusByteArray::Write(dbus::MessageWriter* writer) const {
  writer->AppendArrayOfBytes(value_->front(), value_->size());
}

// static
std::string DbusByteArray::GetSignature() {
  return "ay";  // lmao
}

DbusDictionary::DbusDictionary() = default;
DbusDictionary::DbusDictionary(DbusDictionary&& other) = default;
DbusDictionary::~DbusDictionary() = default;

bool DbusDictionary::Put(const std::string& key, DbusVariant&& value) {
  auto it = value_.find(key);
  const bool updated = it == value_.end() || it->second != value;
  value_[key] = std::move(value);
  return updated;
}

void DbusDictionary::Write(dbus::MessageWriter* writer) const {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);
  for (const auto& pair : value_) {
    dbus::MessageWriter dict_entry_writer(nullptr);
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(pair.first);
    pair.second.Write(&dict_entry_writer);
    array_writer.CloseContainer(&dict_entry_writer);
  }
  writer->CloseContainer(&array_writer);
}

// static
std::string DbusDictionary::GetSignature() {
  return "a{sv}";
}
