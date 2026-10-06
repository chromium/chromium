// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_REGISTRY_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_REGISTRY_H_

#include <array>
#include <cstddef>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/types/pass_key.h"
#include "base/values.h"

class SigninPrefsAccessor;

// Schema of the per-account preferences stored by `SigninPrefs`: which keys
// exist, how they are nested, and what type each one holds.
//
// Access is restricted via the `PassKey` below and by `visibility` on the GN
// target. Other code must use the typed `SigninPrefs` accessors.
class SigninPrefsRegistry {
 public:
  // Listing `SigninPrefsRegistry` itself is what lets
  // `CreatePassKeyForTesting()` mint a key: `base::PassKey<T>` befriends `T`,
  // and a single-holder key converts to a multi-holder one.
  using PassKey = base::PassKey<SigninPrefsAccessor, SigninPrefsRegistry>;

  static PassKey CreatePassKeyForTesting() {
    return base::PassKey<SigninPrefsRegistry>();
  }

  // Maximum number of enclosing dictionaries a registered pref can have. Only
  // the registry's fixed-capacity storage is bounded by this; the accessor
  // accepts any depth. Registering a deeper pref fails to compile in
  // `ToParentStorage()`; raise this value to allow it.
  static constexpr size_t kMaxParentDepth = 2;

  // Describes a single per-account preference.
  struct PrefDescriptor {
    // A single dictionary key. May itself contain '.', which is NOT a
    // separator anywhere in this API.
    std::string_view key;
    // Enclosing dictionary keys, outermost first. Empty for top-level prefs.
    // Stored as a fixed-capacity `std::array` rather than `base::span` because
    // `raw-ptr-plugin` (`check-span-fields`) forbids `base::span` members and
    // `base::raw_span` is not `constexpr`.
    std::array<std::string_view, kMaxParentDepth> parent_keys_storage = {};
    base::Value::Type type = base::Value::Type::NONE;
    bool is_timestamp = false;

    constexpr base::span<const std::string_view> parent_keys() const
        LIFETIME_BOUND {
      size_t count = 0;
      while (count < parent_keys_storage.size() &&
             !parent_keys_storage[count].empty()) {
        ++count;
      }
      return base::span(parent_keys_storage).first(count);
    }
  };

  // Returns all registered per-account preferences.
  static base::span<const PrefDescriptor> GetAll(PassKey);

  // Returns the descriptor for `(parents, key)`, or null if it is not
  // registered. The returned pointer points into static storage.
  static const PrefDescriptor* Find(PassKey,
                                    base::span<const std::string_view> parents,
                                    std::string_view key);

  // Returns true if `(parents, key)` is a registered leaf preference (not a
  // `DICT` container), `value` matches its `base::Value::Type`, and `value`
  // parses as a `base::Time` when `is_timestamp` is true.
  static bool IsValidValue(PassKey,
                           base::span<const std::string_view> parents,
                           std::string_view key,
                           const base::Value& value);
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_REGISTRY_H_
