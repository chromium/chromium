// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/local_state_helper_impl.h"

#include "components/prefs/pref_service.h"

namespace segmentation_platform {

// static
LocalStateHelper& LocalStateHelper::GetInstance() {
  static base::NoDestructor<LocalStateHelperImpl> instance;
  return *instance;
}

LocalStateHelperImpl::LocalStateHelperImpl() = default;

LocalStateHelperImpl::~LocalStateHelperImpl() = default;

void LocalStateHelperImpl::Initialize(PrefService* local_state) {
  local_state_ = local_state;
}

void LocalStateHelperImpl::SetPrefTime(const char* pref_name, base::Time time) {
  if (local_state_) {
    local_state_->SetTime(pref_name, time);
  }
}

base::Time LocalStateHelperImpl::GetPrefTime(const char* pref_name) const {
  return local_state_ ? local_state_->GetTime(pref_name) : base::Time::Max();
}

}  // namespace segmentation_platform
