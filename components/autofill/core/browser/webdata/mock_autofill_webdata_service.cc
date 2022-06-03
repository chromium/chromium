// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"

#include "base/threading/thread_task_runner_handle.h"

namespace autofill {

MockAutofillWebDataService::MockAutofillWebDataService()
    : AutofillWebDataService(base::ThreadTaskRunnerHandle::Get(),
                             base::ThreadTaskRunnerHandle::Get()) {}

MockAutofillWebDataService::~MockAutofillWebDataService() = default;

}  // namespace autofill
