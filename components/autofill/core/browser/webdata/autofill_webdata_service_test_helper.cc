// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"

namespace autofill {

AutofillWebDataServiceTestHelper::AutofillWebDataServiceTestHelper(
    std::unique_ptr<WebDatabaseTable> table,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : ui_task_runner_(ui_task_runner
                          ? ui_task_runner
                          : base::SingleThreadTaskRunner::GetCurrentDefault()),
      db_task_runner_(db_task_runner
                          ? db_task_runner
                          : base::SingleThreadTaskRunner::GetCurrentDefault()) {
  wdbs_->AddTable(std::move(table));
  wdbs_->LoadDatabase(os_crypt_.get());
  awds_->Init(base::NullCallback());
  WaitUntilIdle();
}

AutofillWebDataServiceTestHelper::~AutofillWebDataServiceTestHelper() {
  wdbs_->ShutdownDatabase();
}

void AutofillWebDataServiceTestHelper::WaitUntilIdle() {
  {
    base::RunLoop run_loop;
    db_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run(FROM_HERE);
  }
  {
    base::RunLoop run_loop;
    ui_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run(FROM_HERE);
  }
}

}  // namespace autofill
