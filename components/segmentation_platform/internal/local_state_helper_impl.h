// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_LOCAL_STATE_HELPER_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_LOCAL_STATE_HELPER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/local_state_helper.h"

class PrefService;

namespace segmentation_platform {

// Implementation of the LocalStateHelper class.
class LocalStateHelperImpl : public LocalStateHelper {
 public:
  LocalStateHelperImpl(const LocalStateHelperImpl&) = delete;
  LocalStateHelperImpl& operator=(const LocalStateHelperImpl&) = delete;

  // LocalStateHelper implementation.
  void Initialize(PrefService* local_state) override;
  void SetPrefTime(const char* pref_name, base::Time time) override;
  base::Time GetPrefTime(const char* pref_name) const override;

 private:
  friend base::NoDestructor<LocalStateHelperImpl>;
  LocalStateHelperImpl();
  ~LocalStateHelperImpl() override;

  raw_ptr<PrefService, DanglingUntriaged> local_state_ = nullptr;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_LOCAL_STATE_HELPER_IMPL_H_