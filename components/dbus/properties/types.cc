// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/properties/types.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "dbus/message.h"

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

bool DbusType::operator!=(const DbusType& other) const {
  return !(*this == other);
}

bool DbusType::Move(DbusType&& object) {
  if (!TypeMatches(object)) {
    return false;
  }
  MoveImpl(std::move(object));
  return true;
}

bool DbusType::IsUntyped() const {
  return false;
}

bool DbusType::TypeMatches(const DbusType& other) const {
  return GetSignatureDynamic() == other.GetSignatureDynamic();
}

namespace detail {

UntypedDbusContainer::UntypedDbusContainer() = default;

UntypedDbusContainer::UntypedDbusContainer(
    UntypedDbusContainer&& other) noexcept = default;
UntypedDbusContainer& UntypedDbusContainer::operator=(
    UntypedDbusContainer&& other) noexcept = default;
UntypedDbusContainer::~UntypedDbusContainer() = default;

void UntypedDbusContainer::Write(dbus::MessageWriter* writer) const {
  // Dynamic types are read from variants. Only static types should be written.
  NOTREACHED();
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

// static
std::string DbusDictionary::GetSignature() {
  return "a{sv}";
}

DbusDictionary MakeDbusDictionary() {
  return DbusDictionary();
}
