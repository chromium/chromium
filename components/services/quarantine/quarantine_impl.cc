// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine_impl.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/services/quarantine/quarantine.h"

namespace quarantine {

namespace {

void ReplyToCallback(scoped_refptr<base::TaskRunner> task_runner,
                     mojom::Quarantine::QuarantineFileCallback callback,
                     QuarantineFileResult result) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace

QuarantineImpl::QuarantineImpl() = default;

QuarantineImpl::QuarantineImpl(
    mojo::PendingReceiver<mojom::Quarantine> receiver)
    : receiver_(this, std::move(receiver)) {}

QuarantineImpl::~QuarantineImpl() = default;

void QuarantineImpl::QuarantineFile(
    const base::FilePath& full_path,
    const GURL& source_url,
    const GURL& referrer_url,
    const std::optional<url::Origin>& request_initiator,
    const std::string& client_guid,
    mojom::Quarantine::QuarantineFileCallback callback) {
#if BUILDFLAG(IS_MAC)
  // On Mac posting to a new task runner to do the potentially blocking
  // quarantine work.
  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
#else   // BUILDFLAG(IS_MAC)
  scoped_refptr<base::TaskRunner> task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
#endif  // BUILDFLAG(IS_MAC)
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &quarantine::QuarantineFile, full_path, source_url, referrer_url,
          request_initiator, client_guid,
          base::BindOnce(&ReplyToCallback,
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(callback))));
}

}  // namespace quarantine
