// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_TEST_HELPER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_TEST_HELPER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/webdata/common/web_database_service.h"

namespace autofill {

class AutofillWebDataServiceTestHelper {
 public:
  explicit AutofillWebDataServiceTestHelper(
      std::unique_ptr<WebDatabaseTable> table,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner = nullptr,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner = nullptr);

  ~AutofillWebDataServiceTestHelper();

  scoped_refptr<WebDatabaseService> web_database_service() const {
    return wdbs_;
  }
  scoped_refptr<AutofillWebDataService> autofill_webdata_service() const {
    return awds_;
  }

  // Blocks until the pending database operations and their responses have been
  // completed.
  void WaitUntilIdle();

 private:
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_ =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true);

  scoped_refptr<WebDatabaseService> wdbs_ =
      base::MakeRefCounted<WebDatabaseService>(
          base::FilePath(WebDatabase::kInMemoryPath),
          ui_task_runner_,
          db_task_runner_);

  scoped_refptr<AutofillWebDataService> awds_ =
      base::MakeRefCounted<AutofillWebDataService>(wdbs_, ui_task_runner_);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_TEST_HELPER_H_
