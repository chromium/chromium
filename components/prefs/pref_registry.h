// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_REGISTRY_H_
#define COMPONENTS_PREFS_PREF_REGISTRY_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/prefs_export.h"
#include "components/prefs/transparent_unordered_string_map.h"

namespace base {
class Value;
}

class DefaultPrefStore;
class PrefStore;

// Preferences need to be registered with a type and default value
// before they are used.
//
// The way you use a PrefRegistry is that you register all required
// preferences on it (via one of its subclasses), then pass it as a
// construction parameter to PrefService.
//
// Currently, registrations after constructing the PrefService will
// also work, but this is being deprecated.
class COMPONENTS_PREFS_EXPORT PrefRegistry
    : public base::RefCounted<PrefRegistry> {
 public:
  // Registration flags that can be specified which impact how the pref will
  // behave or be stored. This will be passed in a bitmask when the pref is
  // registered. Subclasses of PrefRegistry can specify their own flags. Care
  // must be taken to ensure none of these overlap with the flags below.
  using PrefRegistrationFlags = uint32_t;

  // No flags are specified.
  static constexpr PrefRegistrationFlags NO_REGISTRATION_FLAGS = 0;

  // The first 8 bits are reserved for subclasses of PrefRegistry to use.

  // This marks the pref as "lossy". There is no strict time guarantee on when
  // a lossy pref will be persisted to permanent storage when it is modified.
  static constexpr PrefRegistrationFlags LOSSY_PREF = 1 << 8;

  // Registering a pref as public allows other services to access it.
  static constexpr PrefRegistrationFlags PUBLIC = 1 << 9;

  using const_iterator = PrefValueMap::const_iterator;
  using PrefRegistrationFlagsMap = TransparentUnorderedStringMap<uint32_t>;

  PrefRegistry();

  PrefRegistry(const PrefRegistry&) = delete;
  PrefRegistry& operator=(const PrefRegistry&) = delete;

  // Retrieve the set of registration flags for the given preference. The return
  // value is a bitmask of PrefRegistrationFlags.
  uint32_t GetRegistrationFlags(std::string_view pref_name) const;

  // Gets the registered defaults.
  scoped_refptr<PrefStore> defaults();

  // Allows iteration over defaults.
  const_iterator begin() const;
  const_iterator end() const;

  // Changes the default value for a preference.
  //
  // `pref_name` must be a previously registered preference.
  void SetDefaultPrefValue(std::string_view pref_name, base::Value value);

 protected:
  friend class base::RefCounted<PrefRegistry>;
  virtual ~PrefRegistry();

  // Used by subclasses to register a default value and registration flags for
  // a preference. `flags` is a bitmask of `PrefRegistrationFlags`.
  void RegisterPreference(std::string_view path,
                          base::Value default_value,
                          uint32_t flags);

  // Allows subclasses to hook into pref registration.
  virtual void OnPrefRegistered(std::string_view path, uint32_t flags);

  scoped_refptr<DefaultPrefStore> defaults_;

  // A map of pref name to a bitmask of PrefRegistrationFlags.
  PrefRegistrationFlagsMap registration_flags_;
};

#endif  // COMPONENTS_PREFS_PREF_REGISTRY_H_
