// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/scoped_user_pref_update.h"

#include "base/logging.h"
#include "components/prefs/pref_notifier.h"
#include "components/prefs/pref_service.h"

namespace subtle {

ScopedUserPrefUpdateBase::ScopedUserPrefUpdateBase(PrefService* service,
                                                   const std::string& path)
    : service_(service), path_(path), value_(nullptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(service_->sequence_checker_);
}

ScopedUserPrefUpdateBase::~ScopedUserPrefUpdateBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Notify();
}

base::Value* ScopedUserPrefUpdateBase::GetValueOfType(base::Value::Type type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!value_)
    value_ = service_->GetMutableUserPref(path_, type);

  // |value_| might be downcast to base::DictionaryValue or base::ListValue,
  // side-stepping CHECKs built into base::Value. Thus we need to be certain
  // that the type matches.
  if (value_)
    CHECK_EQ(value_->type(), type);
  return value_;
}

void ScopedUserPrefUpdateBase::Notify() {
  if (value_) {
    service_->ReportUserPrefChanged(path_);
    value_ = nullptr;
  }
}

}  // namespace subtle
