// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_H_

#include <string>

#include "base/time/time.h"

namespace variations {

// Class to encapsulate details of reading and modifying safe seed state.
//
// This is pure virtual so that different platforms / contexts (e.g. CrOS
// early-boot) can provide their own implementations that will exhibit
// consistent behavior with safe seed usage, and to make sure that if one
// implementation is updated, all others are too.
//
// All reading and writing to VariationsSafeSeed* prefs should go through a
// subclass of VariationsSafeSeedStore.
class VariationsSafeSeedStore {
 public:
  virtual ~VariationsSafeSeedStore() = default;
  // Getter and setter for the time at which the safe seed was persisted to the
  // underlying storage.
  virtual base::Time GetFetchTime() const = 0;
  virtual void SetFetchTime(const base::Time& fetch_time) = 0;

  // Getter and setter for the milestone that was used for the safe seed.
  virtual int GetMilestone() const = 0;
  virtual void SetMilestone(int milestone) = 0;

  // Getter and setter for the last server-provided safe seed date of when the
  // seed to be used was fetched. (See GetTimeForStudyDateChecks.)
  virtual base::Time GetTimeForStudyDateChecks() const = 0;
  virtual void SetTimeForStudyDateChecks(const base::Time& safe_seed_time) = 0;

  // Getter and setter for the compressed, b64-encoded safe seed in the
  // underlying storage.
  virtual std::string GetCompressedSeed() const = 0;
  virtual void SetCompressedSeed(const std::string& safe_compressed) = 0;

  // Getter and setter for the b64-encoded safe seed signature in the
  // underlying storage.
  virtual std::string GetSignature() const = 0;
  virtual void SetSignature(const std::string& safe_seed_signature) = 0;

  // Getter and setter for the locale associated with the safe seed in the
  // underlying storage.
  virtual std::string GetLocale() const = 0;
  virtual void SetLocale(const std::string& locale) = 0;

  // Getter and setter for the permanent consistency country associated with the
  // safe seed in the underlying storage.
  virtual std::string GetPermanentConsistencyCountry() const = 0;
  virtual void SetPermanentConsistencyCountry(
      const std::string& permanent_consistency_country) = 0;

  // Getter and setter for the session consistency country associated with the
  // safe seed in the underlying storage.
  virtual std::string GetSessionConsistencyCountry() const = 0;
  virtual void SetSessionConsistencyCountry(
      const std::string& session_consistency_country) = 0;

  // Clear all state in the underlying storage.
  virtual void ClearState() = 0;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_H_
