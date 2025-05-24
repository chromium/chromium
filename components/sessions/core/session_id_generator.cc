// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_id_generator.h"

#include <ostream>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace sessions {
namespace {

const char kLastValuePref[] = "session_id_generator_last_value";
// On startup, we increment the internal counter by |kCautionaryIdPadding| to
// mitigate issues during ungraceful shutdown, where prefs might not have
// managed to persist the latest generated session IDs, but other databases
// (e.g. sync) might have stored those IDs.
const int kCautionaryIdPadding = 50;

SessionID::id_type DefaultRandGenerator() {
  return base::RandGenerator(std::numeric_limits<SessionID::id_type>::max());
}

}  // namespace

// static
SessionIdGenerator* SessionIdGenerator::GetInstance() {
  return base::Singleton<SessionIdGenerator>::get();
}

// static
void SessionIdGenerator::RegisterPrefs(PrefRegistrySimple* prefs) {
  prefs->RegisterInt64Pref(kLastValuePref, 0);
}

void SessionIdGenerator::Init(PrefService* local_state) {
  DCHECK(local_state);
  DCHECK(!local_state_);
  DCHECK_EQ(0, last_value_) << "NewUnique() used before Init()";

  local_state_ = local_state;
  last_value_ = local_state_->GetInt64(kLastValuePref);
  if (last_value_ <= 0) {
    last_value_ = rand_generator_.Run();
  }

  // Increment by a constant to mitigate issues during ungraceful shutdown,
  // where prefs might not have managed to persist the latest generated session
  // IDs, but other databases (e.g. sync) might have stored those IDs.
  IncrementValueBy(kCautionaryIdPadding);
}

void SessionIdGenerator::Shutdown() {
  local_state_ = nullptr;
  last_value_ = 0;
}

SessionID SessionIdGenerator::NewUnique() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncrementValueBy(1);
  DCHECK(SessionID::IsValidValue(last_value_));
  return SessionID::FromSerializedValue(last_value_);
}

void SessionIdGenerator::SetHighestRestoredID(SessionID highest_restored_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SessionID::id_type difference = 0;
  if (last_value_ < highest_restored_id.id() &&
      highest_restored_id.id() - last_value_ <
          std::numeric_limits<SessionID::id_type>::max() / 2) {
    // Only do the check if the two numbers are not too far apart to prevent
    // issues when the `last_value_` is looping back to 0.
    difference = highest_restored_id.id() - last_value_;
    IncrementValueBy(difference);
  }
  base::UmaHistogramCounts100("Session.ID.RestoredDifference", difference);
}

// static
std::string SessionIdGenerator::GetLastValuePrefNameForTest() {
  return kLastValuePref;
}

void SessionIdGenerator::SetRandomGeneratorForTest(
    const RandomGenerator& rand_generator) {
  rand_generator_ = rand_generator;
}

SessionIdGenerator::SessionIdGenerator()
    : local_state_(nullptr),
      last_value_(0),
      rand_generator_(base::BindRepeating(&DefaultRandGenerator)) {}

SessionIdGenerator::~SessionIdGenerator() = default;

void SessionIdGenerator::IncrementValueBy(int increment) {
  DCHECK_LT(0, increment);
  DCHECK_LE(0, last_value_);
  if (last_value_ >
      std::numeric_limits<SessionID::id_type>::max() - increment) {
    last_value_ = 0;
  }
  last_value_ += increment;
  // Init() should have been called in production (which initializes
  // |local_state_|), but for test convenience, we allow operating even without
  // underlying prefs.
  if (local_state_) {
    local_state_->SetInt64(kLastValuePref, last_value_);
  }
}

bool SessionIdGenerator::IsInitializedForTest() const {
  return local_state_ != nullptr;
}

}  // namespace sessions
