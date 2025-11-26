// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/scoped_variations_ids_provider.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/variations/variations_ids_provider.h"

namespace variations::test {

namespace {

// A clock that returns the time from the ScopedVariationsIdsProvider that owns
// it, if set. Otherwise, returns base::Time::Now().
class TestClock : public base::Clock {
 public:
  explicit TestClock(ScopedVariationsIdsProvider* scoped_provider)
      : scoped_provider_(scoped_provider) {}
  ~TestClock() override = default;

  TestClock(const TestClock&) = delete;
  TestClock& operator=(const TestClock&) = delete;

  base::Time Now() const override {
    return scoped_provider_->time_for_testing.has_value()
               ? scoped_provider_->time_for_testing.value()
               : base::Time::Now();
  }

 private:
  raw_ptr<ScopedVariationsIdsProvider> scoped_provider_;
};

}  // namespace

//
ScopedVariationsIdsProvider::ScopedVariationsIdsProvider(
    VariationsIdsProvider::Mode mode)
    : previous_instance_(VariationsIdsProvider::CreateInstanceForTesting(
          mode,
          std::make_unique<TestClock>(this))),
      current_instance_(VariationsIdsProvider::GetInstance()) {}

ScopedVariationsIdsProvider::~ScopedVariationsIdsProvider() {
  CHECK_EQ(current_instance_, VariationsIdsProvider::GetInstance());
  // Release our pointer to the current instance before destroying it, as
  // `ResetInstanceForTesting()` will delete the global instance and reset
  // the global instance pointer to the previous instance.
  current_instance_ = nullptr;
  VariationsIdsProvider::ResetInstanceForTesting(previous_instance_);
}

VariationsIdsProvider* ScopedVariationsIdsProvider::operator->() {
  CHECK_EQ(current_instance_, VariationsIdsProvider::GetInstance());
  return current_instance_;
}

VariationsIdsProvider& ScopedVariationsIdsProvider::operator*() {
  CHECK_EQ(current_instance_, VariationsIdsProvider::GetInstance());
  return *current_instance_;
}

}  // namespace variations::test
