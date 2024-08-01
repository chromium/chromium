// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/scoped_user_pref_update.h"

#include <string_view>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "components/prefs/pref_notifier.h"
#include "components/prefs/pref_service.h"

// TODO(crbug.com/40895218): The following two can be removed after resolving
// the problem.
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/types/cxx23_to_underlying.h"

namespace subtle {

ScopedUserPrefUpdateBase::ScopedUserPrefUpdateBase(PrefService* service,
                                                   std::string_view path)
    : service_(CHECK_DEREF(service)), path_(path) {
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
  if (!value_) {
    // TODO(crbug.com/40895218) This is unexpected, so let's collect some data.
    const PrefService::Preference* pref = service_->FindPreference(path_);
    SCOPED_CRASH_KEY_NUMBER(
        "ScopedUserPrefUpdate", "PrevServiceStatus",
        base::to_underlying(service_->GetInitializationStatus()));
    SCOPED_CRASH_KEY_STRING32("ScopedUserPrefUpdate", "FindPreference",
                              pref ? "Yes" : "No");
    SCOPED_CRASH_KEY_NUMBER("ScopedUserPrefUpdate", "Type",
                            pref ? base::to_underlying(pref->GetType()) : -1);
    base::debug::DumpWithoutCrashing();
  }
  return value_;
}

void ScopedUserPrefUpdateBase::Notify() {
  if (value_) {
    service_->ReportUserPrefChanged(path_);
    value_ = nullptr;
  }
}

}  // namespace subtle

base::Value::Dict& ScopedDictPrefUpdate::Get() {
  return GetValueOfType(base::Value::Type::DICT)->GetDict();
}

base::Value::List& ScopedListPrefUpdate::Get() {
  return GetValueOfType(base::Value::Type::LIST)->GetList();
}
