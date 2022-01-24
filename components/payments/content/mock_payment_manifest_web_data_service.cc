// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/mock_payment_manifest_web_data_service.h"

#include "base/files/file_path.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/webdata/common/web_database_service.h"

namespace payments {

MockPaymentManifestWebDataService::MockPaymentManifestWebDataService()
    : payments::PaymentManifestWebDataService(
          base::MakeRefCounted<WebDatabaseService>(
              base::FilePath(),
              base::ThreadTaskRunnerHandle::Get(),
              base::ThreadTaskRunnerHandle::Get()),
          base::ThreadTaskRunnerHandle::Get()) {}

MockPaymentManifestWebDataService::~MockPaymentManifestWebDataService() =
    default;

MockWebDataServiceWrapper::MockWebDataServiceWrapper() = default;
MockWebDataServiceWrapper::~MockWebDataServiceWrapper() = default;

}  // namespace payments
