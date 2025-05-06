// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_PROPERTIES_TYPES_H_
#define COMPONENTS_DBUS_PROPERTIES_TYPES_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "dbus/message.h"

namespace base {
class RefCountedMemory;
}

class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusType {
 public:
  virtual ~DbusType();

  bool operator==(const DbusType& other) const;

  // Serializes this object to `writer`.
  virtual void Write(dbus::MessageWriter* writer) const = 0;

  // Deserializes this object from `reader`.
  virtual bool Read(dbus::MessageReader* reader) = 0;

  // Both a virtual and a static version of GetSignature() are necessary.
  // The virtual version is needed by DbusVariant which needs to know the
  // signature at runtime since you could eg. have an array of variants of
  // heterogeneous types.  The static version is needed by DbusArray: if the
  // array is empty, then there would be no DbusType instance to get the
  // signature from.
  virtual std::string GetSignatureDynamic() const = 0;

  // Move from `other` to `this`. UntypedDbusContainer will be converted to
  // typed containers. If types are incompatible, this is a no-op and returns
  // false. Otherwise returns true.
  bool Move(DbusType&& object);

  // True iff this is an UntypedDbusContainer.
  virtual bool IsUntyped() const;

  // True iff this is a DbusParameters.
  virtual bool IsParameters() const;

  // True iff `this` and `other` have exactly the same type.
  bool TypeMatches(const DbusType& other) const;

 protected:
  // This is only safe to call after verifying GetSignatureDynamic() matches.
  virtual bool IsEqual(const DbusType& other_type) const = 0;

  // This is only safe to call after verifying GetSignatureDynamic() matches.
  virtual void MoveImpl(DbusType&& object) = 0;
};

template <typename T>
class DbusTypeImpl : public DbusType {
 public:
  ~DbusTypeImpl() override = default;

  std::string GetSignatureDynamic() const override { return T::GetSignature(); }

  const auto& value() const { return static_cast<const T*>(this)->value_; }
  auto& value() { return static_cast<T*>(this)->value_; }
  void set_value(const auto& new_value) {
    static_cast<T*>(this)->value_ = new_value;
  }

 protected:
  // DbusType:
  bool IsEqual(const DbusType& other_type) const override {
    const T* other = static_cast<const T*>(&other_type);
    return static_cast<const T*>(this)->value_ == other->value_;
  }

  void MoveImpl(DbusType&& object) override {
    T* other = static_cast<T*>(&object);
    static_cast<T*>(this)->value_ = std::move(other->value_);
  }
};

namespace detail {

// It's not possible to read templated types (arrays, structs, and dict
// entries) from variants since it would require knowing the type at compile
// time.  This class exists as a temporary storage for containers read from
// variants.  Calling code which instantiates the templates can then convert
// from this storage to the real type in DbusVariant::GetAs<>().  This is an
// implementation detail and should not be used by client code.
class UntypedDbusContainer final : public DbusType {
 public:
  UntypedDbusContainer();
  UntypedDbusContainer(std::vector<std::unique_ptr<DbusType>> value,
                       std::string signature);
  UntypedDbusContainer(UntypedDbusContainer&& other) noexcept;
  UntypedDbusContainer& operator=(UntypedDbusContainer&& other) noexcept;
  ~UntypedDbusContainer() override;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;
  bool Read(dbus::MessageReader* reader) override;
  std::string GetSignatureDynamic() const override;
  bool IsUntyped() const override;

  std::vector<std::unique_ptr<DbusType>>& value() { return value_; }

 private:
  // DbusType:
  bool IsEqual(const DbusType& other_type) const override;
  void MoveImpl(DbusType&& object) override;

  std::vector<std::unique_ptr<DbusType>> value_;
  std::string signature_;
};

}  // namespace detail

template <typename T,
          typename PassT,
          void (dbus::MessageWriter::*WriteFn)(PassT),
          bool (dbus::MessageReader::*ReadFn)(T*),
          char Signature>
class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusPrimitiveType final
    : public DbusTypeImpl<
          DbusPrimitiveType<T, PassT, WriteFn, ReadFn, Signature>> {
 public:
  DbusPrimitiveType() = default;
  explicit DbusPrimitiveType(T value) : value_{value} {}
  DbusPrimitiveType(DbusPrimitiveType&& other) noexcept = default;
  DbusPrimitiveType& operator=(DbusPrimitiveType&& other) noexcept = default;
  ~DbusPrimitiveType() override = default;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override {
    std::invoke(WriteFn, writer, value_);
  }

  bool Read(dbus::MessageReader* reader) override {
    return std::invoke(ReadFn, reader, &value_);
  }

  static std::string GetSignature() { return std::string(1, Signature); }

 private:
  friend class DbusTypeImpl<
      DbusPrimitiveType<T, PassT, WriteFn, ReadFn, Signature>>;

  T value_ = {};
};

#define DEFINE_DBUS_TYPE(TYPE, PRIMITIVE, PASS_PRIMITIVE, WRITE, READ, \
                         SIGNATURE)                                    \
  template class COMPONENT_EXPORT(COMPONENTS_DBUS)                     \
      DbusPrimitiveType<PRIMITIVE, PASS_PRIMITIVE,                     \
                        &dbus::MessageWriter::WRITE,                   \
                        &dbus::MessageReader::READ, SIGNATURE>;        \
  using TYPE = DbusPrimitiveType<PRIMITIVE, PASS_PRIMITIVE,            \
                                 &dbus::MessageWriter::WRITE,          \
                                 &dbus::MessageReader::READ, SIGNATURE>

#define DEFINE_DBUS_VALUE_TYPE(TYPE, PRIMITIVE, WRITE, READ, SIGNATURE) \
  DEFINE_DBUS_TYPE(TYPE, PRIMITIVE, PRIMITIVE, WRITE, READ, SIGNATURE)

#define DEFINE_DBUS_REF_TYPE(TYPE, PRIMITIVE, WRITE, READ, SIGNATURE) \
  DEFINE_DBUS_TYPE(TYPE, PRIMITIVE, const PRIMITIVE&, WRITE, READ, SIGNATURE)

DEFINE_DBUS_VALUE_TYPE(DbusByte, uint8_t, AppendByte, PopByte, 'y');
DEFINE_DBUS_VALUE_TYPE(DbusBoolean, bool, AppendBool, PopBool, 'b');
DEFINE_DBUS_VALUE_TYPE(DbusInt16, int16_t, AppendInt16, PopInt16, 'n');
DEFINE_DBUS_VALUE_TYPE(DbusUint16, uint16_t, AppendUint16, PopUint16, 'q');
DEFINE_DBUS_VALUE_TYPE(DbusInt32, int32_t, AppendInt32, PopInt32, 'i');
DEFINE_DBUS_VALUE_TYPE(DbusUint32, uint32_t, AppendUint32, PopUint32, 'u');
DEFINE_DBUS_VALUE_TYPE(DbusInt64, int64_t, AppendInt64, PopInt64, 'x');
DEFINE_DBUS_VALUE_TYPE(DbusUint64, uint64_t, AppendUint64, PopUint64, 't');
DEFINE_DBUS_VALUE_TYPE(DbusDouble, double, AppendDouble, PopDouble, 'd');
DEFINE_DBUS_REF_TYPE(DbusString, std::string, AppendString, PopString, 's');
DEFINE_DBUS_REF_TYPE(DbusObjectPath,
                     dbus::ObjectPath,
                     AppendObjectPath,
                     PopObjectPath,
                     'o');

#undef DEFINE_DBUS_REF_TYPE
#undef DEFINE_DBUS_VALUE_TYPE
#undef DEFINE_DBUS_TYPE

// This is not a DbusTypeImpl because this class stores a base::ScopedFD, but
// the DBus write interface takes an int.
class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusUnixFd final : public DbusType {
 public:
  DbusUnixFd();
  explicit DbusUnixFd(base::ScopedFD fd);
  DbusUnixFd(DbusUnixFd&& other) noexcept;
  DbusUnixFd& operator=(DbusUnixFd&& other) noexcept;
  ~DbusUnixFd() override;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;
  bool Read(dbus::MessageReader* reader) override;
  std::string GetSignatureDynamic() const override;
  bool IsUntyped() const override;

  static std::string GetSignature();

  int value() const { return value_.get(); }

 protected:
  // DbusType:
  bool IsEqual(const DbusType& other_type) const override;
  void MoveImpl(DbusType&& object) override;

 private:
  base::ScopedFD value_;
};

class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusVariant final
    : public DbusTypeImpl<DbusVariant> {
 public:
  DbusVariant();
  explicit DbusVariant(std::unique_ptr<DbusType> value);
  DbusVariant(DbusVariant&& other) noexcept;
  DbusVariant& operator=(DbusVariant&& other) noexcept;
  ~DbusVariant() override;

  template <typename T>
  T* GetAs() {
    return const_cast<T*>(const_cast<const DbusVariant*>(this)->GetAs<T>());
  }

  template <typename T>
  const T* GetAs() const {
    if (!value_ || value_->GetSignatureDynamic() != T::GetSignature()) {
      return nullptr;
    }
    if (value_->IsUntyped()) {
      auto value = std::make_unique<T>();
      value->Move(std::move(*value_));
      value_ = std::move(value);
    }
    return static_cast<T*>(value_.get());
  }

  explicit operator bool() const;

  // DbusType:
  bool IsEqual(const DbusType& other_type) const override;
  void Write(dbus::MessageWriter* writer) const override;
  bool Read(dbus::MessageReader* reader) override;

  static std::string GetSignature();

 private:
  friend class DbusTypeImpl<DbusVariant>;

  mutable std::unique_ptr<DbusType> value_;
};

template <typename T>
DbusVariant MakeDbusVariant(T&& t) {
  return DbusVariant{std::make_unique<T>(std::move(t))};
}

template <typename T>
class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusArray final
    : public DbusTypeImpl<DbusArray<T>> {
 public:
  DbusArray() = default;
  explicit DbusArray(std::vector<T>&& value) : value_{std::move(value)} {}
  DbusArray(DbusArray<T>&& other) noexcept = default;
  DbusArray<T>& operator=(DbusArray<T>&& other) noexcept = default;
  ~DbusArray() override = default;

  template <typename... Ts>
  explicit DbusArray(Ts&&... ts) {
    value_.reserve(sizeof...(ts));
    int dummy[] = {0, (value_.push_back(std::move(ts)), 0)...};
    (void)dummy;
  }

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override {
    dbus::MessageWriter array_writer(nullptr);
    writer->OpenArray(T::GetSignature(), &array_writer);
    for (const T& t : value_) {
      t.Write(&array_writer);
    }
    writer->CloseContainer(&array_writer);
  }

  bool Read(dbus::MessageReader* reader) override {
    dbus::MessageReader array_reader(nullptr);
    if (!reader->PopArray(&array_reader)) {
      return false;
    }
    while (array_reader.HasMoreData()) {
      T t;
      if (!t.Read(&array_reader)) {
        return false;
      }
      value_.push_back(std::move(t));
    }
    return true;
  }

  void MoveImpl(DbusType&& object) override {
    // The type signature has already been verified.
    if (!object.IsUntyped()) {
      value_ = std::move(static_cast<DbusArray<T>*>(&object)->value_);
      return;
    }
    auto& arr = static_cast<detail::UntypedDbusContainer*>(&object)->value();
    value_.clear();
    value_.resize(arr.size());
    for (size_t i = 0; i < arr.size(); ++i) {
      value_[i].Move(std::move(*arr[i]));
    }
  }

  std::vector<T>& value() { return value_; }
  const std::vector<T>& value() const { return value_; }

  static std::string GetSignature() {
    return std::string("a") + T::GetSignature();
  }

 private:
  friend class DbusTypeImpl<DbusArray<T>>;

  std::vector<T> value_;
};

template <typename... Ts>
auto MakeDbusArray(Ts&&... ts) {
  return DbusArray<std::common_type_t<Ts...>>{std::move(ts)...};
}

// This is the same as DbusArray<DbusByte>.  This class avoids having to create
// a bunch of heavy virtual objects just to wrap individual bytes.
class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusByteArray final
    : public DbusTypeImpl<DbusByteArray> {
 public:
  DbusByteArray();
  explicit DbusByteArray(scoped_refptr<base::RefCountedMemory> value);
  DbusByteArray(DbusByteArray&& other) noexcept;
  DbusByteArray& operator=(DbusByteArray&& other) noexcept;
  ~DbusByteArray() override;

  // DbusType:
  bool IsEqual(const DbusType& other_type) const override;
  void Write(dbus::MessageWriter* writer) const override;
  bool Read(dbus::MessageReader* reader) override;

  static std::string GetSignature();

 private:
  friend class DbusTypeImpl<DbusByteArray>;

  scoped_refptr<base::RefCountedMemory> value_;
};

template <typename... Ts>
class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusStruct final
    : public DbusTypeImpl<DbusStruct<Ts...>> {
 public:
  DbusStruct() = default;
  explicit DbusStruct(Ts&&... ts) : value_{std::move(ts)...} {}
  DbusStruct(DbusStruct<Ts...>&& other) noexcept = default;
  DbusStruct<Ts...>& operator=(DbusStruct<Ts...>&& other) noexcept = default;
  ~DbusStruct() override = default;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override {
    dbus::MessageWriter struct_writer(nullptr);
    writer->OpenStruct(&struct_writer);
    std::apply(
        [&struct_writer](auto&&... args) { (args.Write(&struct_writer), ...); },
        value_);
    writer->CloseContainer(&struct_writer);
  }

  bool Read(dbus::MessageReader* reader) override {
    dbus::MessageReader struct_reader(nullptr);
    if (!reader->PopStruct(&struct_reader)) {
      return false;
    }
    bool success = true;
    std::apply(
        [&struct_reader, &success](auto&&... args) {
          ((success = success && args.Read(&struct_reader)), ...);
        },
        value_);
    return success;
  }

  void MoveImpl(DbusType&& object) override {
    // The type signature has already been verified.
    if (!object.IsUntyped()) {
      value_ = std::move(static_cast<DbusStruct<Ts...>*>(&object)->value_);
      return;
    }
    auto& dyn_struct =
        static_cast<detail::UntypedDbusContainer*>(&object)->value();
    CHECK_EQ(dyn_struct.size(), sizeof...(Ts));

    size_t index = 0;
    std::apply(
        [&](auto&... args) {
          ((args.Move(std::move(*dyn_struct[index++]))), ...);
        },
        value_);
  }

  static std::string GetSignature() {
    return "(" + (Ts::GetSignature() + ... + std::string()) + ")";
  }

 private:
  friend class DbusTypeImpl<DbusStruct<Ts...>>;

  std::tuple<Ts...> value_;
};

template <typename... Ts>
auto MakeDbusStruct(Ts&&... ts) {
  return DbusStruct<Ts...>{std::move(ts)...};
}

// This is similar to DbusStruct, except there's no DBus struct container around
// the parameters. This is meant to be used for parameters to method calls, or
// for return values from method calls or signals.
template <typename... Ts>
class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusParameters final
    : public DbusTypeImpl<DbusParameters<Ts...>> {
 public:
  DbusParameters() = default;
  explicit DbusParameters(Ts&&... ts) : value_{std::move(ts)...} {}
  DbusParameters(DbusParameters<Ts...>&& other) noexcept = default;
  DbusParameters<Ts...>& operator=(DbusParameters<Ts...>&& other) noexcept =
      default;
  ~DbusParameters() override = default;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override {
    std::apply([&writer](auto&&... args) { (args.Write(writer), ...); },
               value_);
  }

  bool Read(dbus::MessageReader* reader) override {
    bool success = true;
    std::apply(
        [&reader, &success](auto&&... args) {
          ((success = success && args.Read(reader)), ...);
        },
        value_);
    return success;
  }

  void MoveImpl(DbusType&& object) override {
    // The type signature has already been verified.
    if (!object.IsUntyped()) {
      value_ = std::move(static_cast<DbusParameters<Ts...>&>(object).value_);
      return;
    }
    auto& dyn_params =
        static_cast<detail::UntypedDbusContainer*>(&object)->value();
    CHECK_EQ(dyn_params.size(), sizeof...(Ts));

    size_t index = 0;
    std::apply(
        [&](auto&... args) {
          ((args.Move(std::move(*dyn_params[index++]))), ...);
        },
        value_);
  }

  bool IsParameters() const override { return true; }

  static std::string GetSignature() {
    return (Ts::GetSignature() + ... + std::string());
  }

 private:
  friend class DbusTypeImpl<DbusParameters<Ts...>>;

  std::tuple<Ts...> value_;
};

// Template specialization for empty parameters.
template <>
class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusParameters<> final
    : public DbusTypeImpl<DbusParameters<>> {
 public:
  DbusParameters() = default;
  DbusParameters(DbusParameters<>&& other) noexcept = default;
  DbusParameters<>& operator=(DbusParameters<>&& other) noexcept = default;
  ~DbusParameters() override = default;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override {}
  bool Read(dbus::MessageReader* reader) override { return true; }
  void MoveImpl(DbusType&& object) override {}
  bool IsParameters() const override { return true; }

  static std::string GetSignature() { return std::string(); }

 private:
  friend class DbusTypeImpl<DbusParameters<>>;

  // Required for DbusTypeImpl.
  std::tuple<> value_;
};

using DbusVoid = DbusParameters<>;

template <typename... Ts>
auto MakeDbusParameters(Ts&&... ts) {
  return DbusParameters<Ts...>{std::move(ts)...};
}

template <typename K, typename V>
class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusDictEntry final
    : public DbusTypeImpl<DbusDictEntry<K, V>> {
 public:
  DbusDictEntry() = default;
  DbusDictEntry(K&& k, V&& v) : value_{std::move(k), std::move(v)} {}
  DbusDictEntry(DbusDictEntry<K, V>&& other) noexcept = default;
  DbusDictEntry<K, V>& operator=(DbusDictEntry<K, V>&& other) noexcept =
      default;
  ~DbusDictEntry() override = default;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override {
    dbus::MessageWriter dict_entry_writer(nullptr);
    writer->OpenDictEntry(&dict_entry_writer);
    value_.first.Write(&dict_entry_writer);
    value_.second.Write(&dict_entry_writer);
    writer->CloseContainer(&dict_entry_writer);
  }

  bool Read(dbus::MessageReader* reader) override {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!reader->PopDictEntry(&dict_entry_reader)) {
      return false;
    }
    if (!value_.first.Read(&dict_entry_reader)) {
      return false;
    }
    if (!value_.second.Read(&dict_entry_reader)) {
      return false;
    }
    return true;
  }

  void MoveImpl(DbusType&& object) override {
    // The type signature has already been verified.
    if (!object.IsUntyped()) {
      value_ = std::move(static_cast<DbusDictEntry<K, V>*>(&object)->value_);
      return;
    }
    auto& dyn_entry =
        static_cast<detail::UntypedDbusContainer*>(&object)->value();
    CHECK_EQ(dyn_entry.size(), 2U);
    value_.first.Move(std::move(*dyn_entry[0]));
    value_.second.Move(std::move(*dyn_entry[1]));
  }

  static std::string GetSignature() {
    return "{" + K::GetSignature() + V::GetSignature() + "}";
  }

 private:
  friend class DbusTypeImpl<DbusDictEntry<K, V>>;

  std::pair<K, V> value_;
};

template <typename K, typename V>
auto MakeDbusDictEntry(K&& k, V&& v) {
  return DbusDictEntry<K, V>{std::forward<K>(k), std::forward<V>(v)};
}

// A convenience class for DbusArray<DbusDictEntry<DbusString, DbusVariant>>,
// which is a common idiom for DBus APIs.  Except this class has some subtle
// differences:
//   1. Duplicate keys are not allowed.
//   2. You cannot control the ordering of keys.  They will always be in sorted
//      order.
class COMPONENT_EXPORT(COMPONENTS_DBUS) DbusDictionary final
    : public DbusTypeImpl<DbusDictionary> {
 public:
  DbusDictionary();
  DbusDictionary(DbusDictionary&& other) noexcept;
  DbusDictionary& operator=(DbusDictionary&& other) noexcept;
  ~DbusDictionary() override;

  // Returns true iff the value corresponding to `key` was updated.
  bool Put(const std::string& key, DbusVariant&& value);

  // Returns nullptr if `key` isn't in the dictionary.
  DbusVariant* Get(const std::string& key);

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;
  bool Read(dbus::MessageReader* reader) override;

  static std::string GetSignature();

  template <typename T>
  bool PutAs(const std::string& key, T&& value) {
    return Put(key, MakeDbusVariant(std::move(value)));
  }

  template <typename T>
  T* GetAs(const std::string& key) {
    DbusVariant* variant = Get(key);
    if (!variant) {
      return nullptr;
    }
    return variant->GetAs<T>();
  }

  template <typename T>
  const T* GetAs(const std::string& key) const {
    return const_cast<DbusDictionary*>(this)->GetAs<T>(key);
  }

 private:
  friend class DbusTypeImpl<DbusDictionary>;

  std::map<std::string, DbusVariant> value_;
};

COMPONENT_EXPORT(COMPONENTS_DBUS)
DbusDictionary MakeDbusDictionary();

template <typename V, typename... Args>
DbusDictionary MakeDbusDictionary(const std::string& key,
                                  V&& value,
                                  Args&&... rest) {
  DbusDictionary dict = MakeDbusDictionary(std::forward<Args>(rest)...);
  dict.PutAs(key, std::forward<V>(value));
  return dict;
}

// Reads all fields of a message. Returns a DbusVariant with the
// following stored values:
//   - For read errors, nullptr
//   - For 0 fields, DbusVoid (aka DbusParameters<>)
//   - For 1 field, the unwrapped type
//   - For 2 or more fields, DbusParameters
COMPONENT_EXPORT(COMPONENTS_DBUS)
DbusVariant ReadDbusMessage(dbus::MessageReader* reader);

#endif  // COMPONENTS_DBUS_PROPERTIES_TYPES_H_
