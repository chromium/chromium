// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"

namespace safe_browsing {

SafeBrowsingServiceInterface::SafeBrowsingServiceInterface() = default;

SafeBrowsingServiceInterface::~SafeBrowsingServiceInterface() = default;

SafeBrowsingServiceInterface*
SafeBrowsingServiceInterface::CreateSafeBrowsingService() {
  return factory_ ? factory_->CreateSafeBrowsingService() : nullptr;
}

base::OnceClosure
SafeBrowsingServiceInterface::TakeAddProfileTasksCompletedClosureForTesting() {
  return std::move(add_profile_tasks_completed_closure_for_testing_);
}

// static
SafeBrowsingServiceFactory* SafeBrowsingServiceInterface::factory_ = nullptr;

}  // namespace safe_browsing
