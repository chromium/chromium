// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error_waiter.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"

namespace test {

GlobalErrorWaiter::GlobalErrorWaiter(Profile* profile) {
  scoped_observation_.Observe(
      GlobalErrorServiceFactory::GetForProfile(profile));
}

GlobalErrorWaiter::~GlobalErrorWaiter() = default;

void GlobalErrorWaiter::OnGlobalErrorsChanged() {
  if (run_loop_.running())
    run_loop_.Quit();
  else
    errors_changed_ = true;
}

void GlobalErrorWaiter::Wait() {
  if (!errors_changed_)
    run_loop_.Run();
}

}  // namespace test
