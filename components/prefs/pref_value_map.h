// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_VALUE_MAP_H_
#define COMPONENTS_PREFS_PREF_VALUE_MAP_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"
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

  // Gets the value for |key| and stores it in |value|. Ownership remains with
  // the map. Returns true if a value is present. If not, |value| is not
  // touched.
  bool GetValue(base::StringPiece key, const base::Value** value) const;
  bool GetValue(base::StringPiece key, base::Value** value);

  // Sets a new |value| for |key|. Returns true if the value changed.
  bool SetValue(const std::string& key, base::Value value);

  // Removes the value for |key| from the map. Returns true if a value was
  // removed.
  bool RemoveValue(const std::string& key);

  // Clears the map.
  void Clear();

  // Clear the preferences which start with |prefix|.
  void ClearWithPrefix(const std::string& prefix);

  // Swaps the contents of two maps.
  void Swap(PrefValueMap* other);

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;
  bool empty() const;

  // Gets a boolean value for |key| and stores it in |value|. Returns true if
  // the value was found and of the proper type.
  bool GetBoolean(const std::string& key, bool* value) const;

  // Sets the value for |key| to the boolean |value|.
  void SetBoolean(const std::string& key, bool value);

  // Gets a string value for |key| and stores it in |value|. Returns true if
  // the value was found and of the proper type.
  bool GetString(const std::string& key, std::string* value) const;

  // Sets the value for |key| to the string |value|.
  void SetString(const std::string& key, const std::string& value);

  // Gets an int value for |key| and stores it in |value|. Returns true if
  // the value was found and of the proper type.
  bool GetInteger(const std::string& key, int* value) const;

  // Sets the value for |key| to the int |value|.
  void SetInteger(const std::string& key, const int value);

  // Sets the value for |key| to the double |value|.
  void SetDouble(const std::string& key, const double value);

  // Compares this value map against |other| and stores all key names that have
  // different values in |differing_keys|. This includes keys that are present
  // only in one of the maps.
  void GetDifferingKeys(const PrefValueMap* other,
                        std::vector<std::string>* differing_keys) const;

  // Copies the map into a Value::Dict.
  base::Value::Dict AsDict() const;

 private:
  Map prefs_;
};

#endif  // COMPONENTS_PREFS_PREF_VALUE_MAP_H_
