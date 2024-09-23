// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_VALUE_MAP_H_
#define COMPONENTS_PREFS_PREF_VALUE_MAP_H_

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "components/prefs/prefs_export.h"

// A generic string to value map used by the PrefStore implementations.
class COMPONENTS_PREFS_EXPORT PrefValueMap {
 public:
  using Map = std::map<std::string, base::Value, std::less<void>>;
  using iterator = Map::iterator;
  using const_iterator = Map::const_iterator;

  PrefValueMap();

  PrefValueMap(const PrefValueMap&) = delete;
  PrefValueMap& operator=(const PrefValueMap&) = delete;

  virtual ~PrefValueMap();

  // Gets the value for `key` and stores it in `value`. Ownership remains with
  // the map. Returns true if a value is present. If not, `value` is not
  // touched.
  bool GetValue(std::string_view key, const base::Value** value) const;
  bool GetValue(std::string_view key, base::Value** value);

  // Sets a new `value` for `key`. Returns true if the value changed.
  bool SetValue(std::string_view key, base::Value value);

  // Removes the value for `key` from the map. Returns true if a value was
  // removed.
  bool RemoveValue(std::string_view key);

  // Clears the map.
  void Clear();

  // Clear the preferences which start with `prefix`.
  void ClearWithPrefix(std::string_view prefix);

  // Swaps the contents of two maps.
  void Swap(PrefValueMap* other);

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;
  bool empty() const;

  // Gets a boolean value for `key` and stores it in `value`. Returns true if
  // the value was found and of the proper type.
  bool GetBoolean(std::string_view key, bool* value) const;

  // Sets the value for `key` to the boolean `value`.
  void SetBoolean(std::string_view key, bool value);

  // Gets a string value for `key` and stores it in `value`. Returns true if
  // the value was found and of the proper type.
  bool GetString(std::string_view key, std::string* value) const;

  // Sets the value for `key` to the string `value`.
  void SetString(std::string_view key, std::string_view value);

  // Gets an int value for `key` and stores it in `value`. Returns true if
  // the value was found and of the proper type.
  bool GetInteger(std::string_view key, int* value) const;

  // Sets the value for `key` to the int `value`.
  void SetInteger(std::string_view key, const int value);

  // Sets the value for `key` to the double `value`.
  void SetDouble(std::string_view key, const double value);

  // Compares this value map against `other` and stores all key names that have
  // different values in `differing_keys`. This includes keys that are present
  // only in one of the maps.
  void GetDifferingKeys(const PrefValueMap* other,
                        std::vector<std::string>* differing_keys) const;

  // Copies the map into a Value::Dict.
  base::Value::Dict AsDict() const;

 private:
  Map prefs_;
};

#endif  // COMPONENTS_PREFS_PREF_VALUE_MAP_H_
