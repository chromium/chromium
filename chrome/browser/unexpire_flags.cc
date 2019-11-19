// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unexpire_flags.h"

#include "base/no_destructor.h"
#include "chrome/browser/expired_flags_list.h"

namespace flags {

namespace {

class FlagPredicateSingleton {
 public:
  FlagPredicateSingleton() = default;
  ~FlagPredicateSingleton() = default;

  static const testing::FlagPredicate& GetPredicate() {
    return GetInstance()->predicate_;
  }
  static void SetPredicate(testing::FlagPredicate predicate) {
    GetInstance()->predicate_ = predicate;
  }

 private:
  static FlagPredicateSingleton* GetInstance() {
    static base::NoDestructor<FlagPredicateSingleton> instance;
    return instance.get();
  }

  testing::FlagPredicate predicate_;
};

}  // namespace

const base::Feature kUnexpireFlagsM78{"TemporaryUnexpireFlagsM78",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kUnexpireFlagsM80{"TemporaryUnexpireFlagsM80",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

bool ExpiryEnabledForMstone(int mstone) {
  // generate_expired_list.py will never emit flags with expiry milestone -1, to
  // keep binary size down. However, if a bug *did* cause that to happen, and
  // this function did not handle that case, disaster could ensue: all the -1
  // flags that are supposed to never expire would in fact expire instantly,
  // since -1 < x for any valid mstone x.
  // As such, there's an extra error-check here: never allow flags with mstone
  // -1 to expire.
  DCHECK(mstone != -1);
  if (mstone == -1)
    return false;

  // In M78 this expired flags with their expirations set to M76 or earlier.
  // In M79 this expires flags with their expirations set to M78 or earlier.
  // In M80 and later, this will expire any flags whose expiration is <= the
  // current mstone, and this block comment will go away along with these
  // special cases.
  if (mstone <= 76)
    return true;
  if (mstone == 77 || mstone == 78)
    return !base::FeatureList::IsEnabled(kUnexpireFlagsM78);
  if (mstone == 79 || mstone == 80)
    return !base::FeatureList::IsEnabled(kUnexpireFlagsM80);
  return false;
}

bool IsFlagExpired(const char* internal_name) {
  if (FlagPredicateSingleton::GetPredicate())
    return FlagPredicateSingleton::GetPredicate().Run(internal_name);

  for (int i = 0; kExpiredFlags[i].name; ++i) {
    const ExpiredFlag* f = &kExpiredFlags[i];
    if (!strcmp(f->name, internal_name) && ExpiryEnabledForMstone(f->mstone))
      return true;
  }
  return false;
}

namespace testing {

void SetFlagExpiredPredicate(FlagPredicate predicate) {
  FlagPredicateSingleton::SetPredicate(predicate);
}

}  // namespace testing

}  // namespace flags
