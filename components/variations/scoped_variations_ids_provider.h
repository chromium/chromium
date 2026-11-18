// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SCOPED_VARIATIONS_IDS_PROVIDER_H_
#define COMPONENTS_VARIATIONS_SCOPED_VARIATIONS_IDS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/variations/variations_ids_provider.h"

namespace variations::test {

// A helper class for tests that need to create a `VariationsIdsProvider`
// instance for the duration of the test.
class ScopedVariationsIdsProvider {
 public:
  // Adopts the existing global `VariationsIdsProvider` and destroys it on
  // scope exit (resetting the global instance pointer to nullptr). Requires a
  // global instance to exist and no previous instance to have been displaced.
  // Note that `time_for_testing` has no effect when adopting an existing
  // instance (since it uses whatever clock it was created with).
  static ScopedVariationsIdsProvider AdoptCurrentInstanceForTesting();

  explicit ScopedVariationsIdsProvider(VariationsIdsProvider::Mode mode);
  ~ScopedVariationsIdsProvider();

  ScopedVariationsIdsProvider(const ScopedVariationsIdsProvider&) = delete;
  ScopedVariationsIdsProvider& operator=(const ScopedVariationsIdsProvider&) =
      delete;

  VariationsIdsProvider* operator->();
  VariationsIdsProvider& operator*();

  // The time used by the provider will default to the current time (via
  // `base::Time::Now()`). This can be overridden for testing by setting this
  // field to some specific time.
  std::optional<base::Time> time_for_testing = std::nullopt;

 private:
  // Adopts `VariationsIdsProvider::GetInstance()`.
  ScopedVariationsIdsProvider();

  // A pointer to the previous instance of `VariationsIdsProvider` that is
  // in place when this class is instantiated. This is used to restore the
  // global instance pointer to the previous instance upon destruction.
  raw_ptr<VariationsIdsProvider> previous_instance_ = nullptr;

  // The scoped instance of `VariationsIdsProvider` that is created and
  // destroyed by this class. This should match the global variations ids
  // provider pointer for the lifetime of this `ScopedVariationsIdsProvider
  // instance, as verified by CHECKs in the implementation.
  raw_ptr<VariationsIdsProvider> current_instance_ = nullptr;
};

}  // namespace variations::test

#endif  // COMPONENTS_VARIATIONS_SCOPED_VARIATIONS_IDS_PROVIDER_H_
