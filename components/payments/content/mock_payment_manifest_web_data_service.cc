// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/mock_payment_manifest_web_data_service.h"

#include "base/files/file_path.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webdata/common/web_database_service.h"

namespace payments {

MockPaymentManifestWebDataService::MockPaymentManifestWebDataService()
    : payments::PaymentManifestWebDataService(
          base::MakeRefCounted<WebDatabaseService>(
              base::FilePath(),
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              base::SingleThreadTaskRunner::GetCurrentDefault()),
          base::SingleThreadTaskRunner::GetCurrentDefault()) {}

MockPaymentManifestWebDataService::~MockPaymentManifestWebDataService() =
    default;

MockWebDataServiceWrapper::MockWebDataServiceWrapper() = default;
MockWebDataServiceWrapper::~MockWebDataServiceWrapper() = default;

}  // namespace payments
