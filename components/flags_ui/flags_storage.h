// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_FLAGS_STORAGE_H_
#define COMPONENTS_FLAGS_UI_FLAGS_STORAGE_H_

#include <set>
#include <string>

namespace flags_ui {

// Base class for flags storage implementations.  Enables the about_flags
// functions to store and retrieve data from various sources like PrefService
// and CrosSettings.
class FlagsStorage {
 public:
  virtual ~FlagsStorage() = default;

  // Retrieves the flags as a set of strings.
  virtual std::set<std::string> GetFlags() const = 0;
  // Stores the |flags| and returns true on success.
  virtual bool SetFlags(const std::set<std::string>& flags) = 0;

  // Retrieves the serialized origin list corresponding to
  // |internal_entry_name|. Does not check if the return value is well formed.
  virtual std::string GetOriginListFlag(
      const std::string& internal_entry_name) const = 0;
  // Sets the serialized |origin_list_value| corresponding to
  // |internal_entry_name|. Does not check if |origin_list_value| is well
  // formed.
  virtual void SetOriginListFlag(const std::string& internal_entry_name,
                                 const std::string& origin_list_value) = 0;

  // Retrieves the serialized origin list corresponding to
  // |internal_entry_name|. Does not check if the return value is well formed.
  virtual std::string GetStringFlag(
      const std::string& internal_entry_name) const = 0;
  // Sets the serialized |origin_list_value| corresponding to
  // |internal_entry_name|. Does not check if |origin_list_value| is well
  // formed.
  virtual void SetStringFlag(const std::string& internal_entry_name,
                             const std::string& string_value) = 0;

  // Lands pending changes to disk immediately.
  virtual void CommitPendingWrites() = 0;
};

}  // namespace flags_ui

#endif  // COMPONENTS_FLAGS_UI_FLAGS_STORAGE_H_
