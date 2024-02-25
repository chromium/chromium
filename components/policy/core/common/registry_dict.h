// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REGISTRY_DICT_H_
#define COMPONENTS_POLICY_CORE_COMMON_REGISTRY_DICT_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "build/build_config.h"
#include "components/policy/policy_export.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base {
class Value;
}

namespace policy {

class Schema;

// Converts a value (as read from the registry) to meet |schema|, converting
// types as necessary. Unconvertible types will show up as null values in the
// result.
std::optional<base::Value> POLICY_EXPORT
ConvertRegistryValue(const base::Value& value, const Schema& schema);

// A case-insensitive string comparison functor.
struct POLICY_EXPORT CaseInsensitiveStringCompare {
  bool operator()(const std::string& a, const std::string& b) const;
};

// In-memory representation of a registry subtree. Using a
// base::Value::Dict directly seems tempting, but that doesn't handle the
// registry's case-insensitive-but-case-preserving semantics properly.
class POLICY_EXPORT RegistryDict {
 public:
  using KeyMap = std::map<std::string,
                          std::unique_ptr<RegistryDict>,
                          CaseInsensitiveStringCompare>;
  using ValueMap =
      std::map<std::string, base::Value, CaseInsensitiveStringCompare>;

  RegistryDict();
  RegistryDict(const RegistryDict&) = delete;
  RegistryDict& operator=(const RegistryDict&) = delete;
  ~RegistryDict();

  // Returns a pointer to an existing key, NULL if not present.
  RegistryDict* GetKey(const std::string& name);
  const RegistryDict* GetKey(const std::string& name) const;
  // Sets a key. If |dict| is NULL, clears that key.
  void SetKey(const std::string& name, std::unique_ptr<RegistryDict> dict);
  // Removes a key. If the key doesn't exist, NULL is returned.
  std::unique_ptr<RegistryDict> RemoveKey(const std::string& name);
  // Clears all keys.
  void ClearKeys();

  // Returns a pointer to a value, NULL if not present.
  base::Value* GetValue(const std::string& name);
  const base::Value* GetValue(const std::string& name) const;
  // Sets a value.
  void SetValue(const std::string& name, base::Value&& value);
  // Removes a value. If the value doesn't exist, nullopt is returned.
  std::optional<base::Value> RemoveValue(const std::string& name);
  // Clears all values.
  void ClearValues();

  // Merge keys and values from |other|, giving precedence to |other|.
  void Merge(const RegistryDict& other);

  // Swap with |other|.
  void Swap(RegistryDict* other);

#if BUILDFLAG(IS_WIN)
  // Read a Windows registry subtree into this registry dictionary object.
  void ReadRegistry(HKEY hive, const std::wstring& root);

  // Converts the dictionary to base::Value representation. For key/value name
  // collisions, the key wins. |schema| is used to determine the expected type
  // for each policy.
  // The underlying data of the returned object is either a base::Value::Dict or
  // a base::Value::List.
  std::optional<base::Value> ConvertToJSON(const class Schema& schema) const;
#endif

  const KeyMap& keys() const { return keys_; }
  const ValueMap& values() const { return values_; }

 private:
  KeyMap keys_;
  ValueMap values_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REGISTRY_DICT_H_
