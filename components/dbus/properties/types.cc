// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/properties/types.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "dbus/message.h"

namespace {

bool PopContainer(dbus::MessageReader* reader,
                  dbus::MessageReader* element_reader) {
  switch (reader->GetDataType()) {
    case dbus::Message::DataType::ARRAY:
      return reader->PopArray(element_reader);
    case dbus::Message::DataType::STRUCT:
      return reader->PopStruct(element_reader);
    case dbus::Message::DataType::DICT_ENTRY:
      return reader->PopDictEntry(element_reader);
    default:
      NOTREACHED();
  }
}

template <typename T>
std::unique_ptr<DbusType> CreateDbusType(dbus::MessageReader* reader) {
  auto value = std::make_unique<T>();
  if (!value->Read(reader)) {
    return nullptr;
  }
  return value;
}

std::unique_ptr<DbusType> CreateDynamicDbusType(dbus::MessageReader* reader) {
  switch (reader->GetDataType()) {
    case dbus::Message::DataType::BYTE:
      return CreateDbusType<DbusByte>(reader);
    case dbus::Message::DataType::BOOL:
      return CreateDbusType<DbusBoolean>(reader);
    case dbus::Message::DataType::INT16:
      return CreateDbusType<DbusInt16>(reader);
    case dbus::Message::DataType::UINT16:
      return CreateDbusType<DbusUint16>(reader);
    case dbus::Message::DataType::INT32:
      return CreateDbusType<DbusInt32>(reader);
    case dbus::Message::DataType::UINT32:
      return CreateDbusType<DbusUint32>(reader);
    case dbus::Message::DataType::INT64:
      return CreateDbusType<DbusInt64>(reader);
    case dbus::Message::DataType::UINT64:
      return CreateDbusType<DbusUint64>(reader);
    case dbus::Message::DataType::DOUBLE:
      return CreateDbusType<DbusDouble>(reader);
    case dbus::Message::DataType::STRING:
      return CreateDbusType<DbusString>(reader);
    case dbus::Message::DataType::OBJECT_PATH:
      return CreateDbusType<DbusObjectPath>(reader);
    case dbus::Message::DataType::UNIX_FD:
      return CreateDbusType<DbusUnixFd>(reader);
    case dbus::Message::DataType::VARIANT:
      return CreateDbusType<DbusVariant>(reader);
    case dbus::Message::DataType::ARRAY:
      // Special case for byte arrays to avoid creating a bunch of virtual
      // objects for each byte of binary data.
      if (reader->GetDataSignature() == "ay") {
        return CreateDbusType<DbusByteArray>(reader);
      }
      // Special case for string-variant dictionaries.
      if (reader->GetDataSignature() == "a{sv}") {
        return CreateDbusType<DbusDictionary>(reader);
      }
      [[fallthrough]];
    case dbus::Message::DataType::STRUCT:
    case dbus::Message::DataType::DICT_ENTRY:
      // For templated types (array, struct dict entry), an untyped container is
      // required.
      return CreateDbusType<detail::UntypedDbusContainer>(reader);
    case dbus::Message::DataType::INVALID_DATA:
      return nullptr;
  }
}

}  // namespace

DbusType::~DbusType() = default;

bool DbusType::operator==(const DbusType& other) const {
  if (!TypeMatches(other)) {
    return false;
  }
  // Dynamic types should be casted to static types before comparison.
  CHECK(!IsUntyped());
  CHECK(!other.IsUntyped());
  return IsEqual(other);
}

bool DbusType::Move(DbusType&& object) {
  if (GetSignatureDynamic() != object.GetSignatureDynamic()) {
    return false;
  }
  // Allow moving from UntypedDbusContainer to DbusParameters.
  if (IsParameters() != object.IsParameters() &&
      (!IsParameters() || !object.IsUntyped())) {
    return false;
  }
  MoveImpl(std::move(object));
  return true;
}

bool DbusType::IsUntyped() const {
  return false;
}

bool DbusType::IsParameters() const {
  return false;
}

bool DbusType::TypeMatches(const DbusType& other) const {
  // An explicit check for IsParameters() is necessary since
  // DbusParameters<DbusInt32> has the same signature as DbusInt32.
  return IsParameters() == other.IsParameters() &&
         GetSignatureDynamic() == other.GetSignatureDynamic();
}

namespace detail {

UntypedDbusContainer::UntypedDbusContainer() = default;

UntypedDbusContainer::UntypedDbusContainer(
    std::vector<std::unique_ptr<DbusType>> value,
    std::string signature)
    : value_(std::move(value)), signature_(std::move(signature)) {}

UntypedDbusContainer::UntypedDbusContainer(
    UntypedDbusContainer&& other) noexcept = default;
UntypedDbusContainer& UntypedDbusContainer::operator=(
    UntypedDbusContainer&& other) noexcept = default;
UntypedDbusContainer::~UntypedDbusContainer() = default;

void UntypedDbusContainer::Write(dbus::MessageWriter* writer) const {
  // Dynamic types are read from variants. Only static types should be written.
  NOTREACHED();
}

bool UntypedDbusContainer::Read(dbus::MessageReader* reader) {
  dbus::MessageReader element_reader(nullptr);
  signature_ = reader->GetDataSignature();
  if (!PopContainer(reader, &element_reader)) {
    return false;
  }
  value_.clear();
  while (element_reader.HasMoreData()) {
    std::unique_ptr<DbusType> element = CreateDynamicDbusType(&element_reader);
    if (!element) {
      return false;
    }
    value_.push_back(std::move(element));
  }
  return true;
}

std::string UntypedDbusContainer::GetSignatureDynamic() const {
  return signature_;
}

bool UntypedDbusContainer::IsEqual(const DbusType& other_type) const {
  // Dynamic types should be casted to static types before comparison.
  NOTREACHED();
}

void UntypedDbusContainer::MoveImpl(DbusType&& object) {
  if (!object.IsUntyped()) {
    // Can't currently move from a typed container back to an untyped one.
    // It's possible to implement, but there's no point.
    NOTIMPLEMENTED();
    return;
  }
  CHECK(object.IsUntyped());
  auto* other = static_cast<UntypedDbusContainer*>(&object);
  value_ = std::move(other->value_);
  signature_ = std::move(other->signature_);
}

bool UntypedDbusContainer::IsUntyped() const {
  return true;
}

}  // namespace detail

DbusUnixFd::DbusUnixFd() = default;

DbusUnixFd::DbusUnixFd(base::ScopedFD fd) : value_(std::move(fd)) {}

DbusUnixFd::DbusUnixFd(DbusUnixFd&& other) noexcept = default;
DbusUnixFd& DbusUnixFd::operator=(DbusUnixFd&& other) noexcept = default;
DbusUnixFd::~DbusUnixFd() = default;

void DbusUnixFd::Write(dbus::MessageWriter* writer) const {
  // The fd will be duplicated.
  writer->AppendFileDescriptor(value_.get());
}

bool DbusUnixFd::Read(dbus::MessageReader* reader) {
  return reader->PopFileDescriptor(&value_);
}

std::string DbusUnixFd::GetSignatureDynamic() const {
  return "h";
}

bool DbusUnixFd::IsEqual(const DbusType& other_type) const {
  // FDs can't be compared, other than by value.  However, since ScopedFD has
  // unique ownership over the FD, values will always compare false.
  return false;
}

void DbusUnixFd::MoveImpl(DbusType&& object) {
  value_ = std::move(static_cast<DbusUnixFd*>(&object)->value_);
}

bool DbusUnixFd::IsUntyped() const {
  return false;
}

// static
std::string DbusUnixFd::GetSignature() {
  return "h";
}

DbusVariant::DbusVariant() = default;

DbusVariant::DbusVariant(std::unique_ptr<DbusType> value)
    : value_(std::move(value)) {}

DbusVariant::DbusVariant(DbusVariant&& other) noexcept = default;
DbusVariant& DbusVariant::operator=(DbusVariant&& other) noexcept = default;
DbusVariant::~DbusVariant() = default;

DbusVariant::operator bool() const {
  return !!value_;
}

bool DbusVariant::IsEqual(const DbusType& other_type) const {
  const DbusVariant* other = static_cast<const DbusVariant*>(&other_type);
  if (value_ && other->value_) {
    return *value_ == *other->value_;
  }
  return value_ == other->value_;
}

void DbusVariant::Write(dbus::MessageWriter* writer) const {
  CHECK(value_);
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant(value_->GetSignatureDynamic(), &variant_writer);
  value_->Write(&variant_writer);
  writer->CloseContainer(&variant_writer);
}

bool DbusVariant::Read(dbus::MessageReader* reader) {
  dbus::MessageReader variant_reader(nullptr);
  if (!reader->PopVariant(&variant_reader)) {
    return false;
  }
  value_ = CreateDynamicDbusType(&variant_reader);
  return value_ != nullptr;
}

// static
std::string DbusVariant::GetSignature() {
  return "v";
}

DbusByteArray::DbusByteArray() = default;
DbusByteArray::DbusByteArray(scoped_refptr<base::RefCountedMemory> value)
    : value_(std::move(value)) {}
DbusByteArray::DbusByteArray(DbusByteArray&& other) noexcept = default;
DbusByteArray& DbusByteArray::operator=(DbusByteArray&& other) noexcept =
    default;
DbusByteArray::~DbusByteArray() = default;

bool DbusByteArray::IsEqual(const DbusType& other_type) const {
  const DbusByteArray* other = static_cast<const DbusByteArray*>(&other_type);
  return value_->Equals(other->value_);
}

void DbusByteArray::Write(dbus::MessageWriter* writer) const {
  writer->AppendArrayOfBytes(*value_);
}

bool DbusByteArray::Read(dbus::MessageReader* reader) {
  const uint8_t* bytes = nullptr;
  size_t length = 0;
  if (!reader->PopArrayOfBytes(&bytes, &length)) {
    return false;
  }
  // SAFETY: the span adapts the pointer-length return value of
  // PopArrayOfBytes() without any pointer arithmetic.
  auto data = UNSAFE_BUFFERS(base::span(bytes, length));
  value_ = base::MakeRefCounted<base::RefCountedBytes>(data);
  return true;
}

// static
std::string DbusByteArray::GetSignature() {
  return "ay";
}

DbusDictionary::DbusDictionary() = default;
DbusDictionary::DbusDictionary(DbusDictionary&& other) noexcept = default;
DbusDictionary& DbusDictionary::operator=(DbusDictionary&& other) noexcept =
    default;
DbusDictionary::~DbusDictionary() = default;

bool DbusDictionary::Put(const std::string& key, DbusVariant&& value) {
  auto it = value_.find(key);
  const bool updated = it == value_.end() || it->second != value;
  value_[key] = std::move(value);
  return updated;
}

DbusVariant* DbusDictionary::Get(const std::string& key) {
  auto it = value_.find(key);
  return it != value_.end() ? &it->second : nullptr;
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

bool DbusDictionary::Read(dbus::MessageReader* reader) {
  dbus::MessageReader array_reader(nullptr);
  if (!reader->PopArray(&array_reader)) {
    return false;
  }
  while (array_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!array_reader.PopDictEntry(&dict_entry_reader)) {
      return false;
    }
    std::string key;
    if (!dict_entry_reader.PopString(&key)) {
      return false;
    }
    DbusVariant value;
    if (!value.Read(&dict_entry_reader)) {
      return false;
    }
    value_[key] = std::move(value);
  }
  return true;
}

// static
std::string DbusDictionary::GetSignature() {
  return "a{sv}";
}

DbusDictionary MakeDbusDictionary() {
  return DbusDictionary();
}

DbusVariant ReadDbusMessage(dbus::MessageReader* reader) {
  std::string signature;
  std::vector<std::unique_ptr<DbusType>> data;
  while (reader->HasMoreData()) {
    data.push_back(CreateDynamicDbusType(reader));
    if (!data.back()) {
      return DbusVariant();
    }
    signature += data.back()->GetSignatureDynamic();
  }
  if (data.size() == 1U) {
    return DbusVariant(std::move(data[0]));
  }
  return DbusVariant(std::make_unique<detail::UntypedDbusContainer>(
      std::move(data), std::move(signature)));
}
