// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include "base/check_deref.h"

namespace autofill {

PersonalContextAccessManagerImpl::PersonalContextAccessManagerImpl(
    personal_context::PersonalContextService* personal_context_service)
    : personal_context_service_(CHECK_DEREF(personal_context_service)) {}

PersonalContextAccessManagerImpl::~PersonalContextAccessManagerImpl() = default;

}  // namespace autofill
