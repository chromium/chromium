// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine_impl.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "build/build_config.h"
#include "components/services/quarantine/quarantine.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "components/services/quarantine/public/cpp/quarantine_features_win.h"
#endif  // OS_WIN

namespace quarantine {

QuarantineImpl::QuarantineImpl() = default;

QuarantineImpl::QuarantineImpl(
    mojo::PendingReceiver<mojom::Quarantine> receiver)
    : receiver_(this, std::move(receiver)) {}

QuarantineImpl::~QuarantineImpl() = default;

namespace {

#if defined(OS_WIN)
scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
  return base::CreateCOMSTATaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}
#else   // OS_WIN
scoped_refptr<base::TaskRunner> GetTaskRunner() {
  return base::CreateTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}
#endif  // OS_WIN

}  // namespace

void QuarantineImpl::QuarantineFile(
    const base::FilePath& full_path,
    const GURL& source_url,
    const GURL& referrer_url,
    const std::string& client_guid,
    mojom::Quarantine::QuarantineFileCallback callback) {
#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(quarantine::kOutOfProcessQuarantine)) {
    // In out of process case, we are running in a utility process,
    // so directly call QuarantineFile and send the result.
    base::win::ScopedCOMInitializer com_initializer;

    QuarantineFileResult result = quarantine::QuarantineFile(
        full_path, source_url, referrer_url, client_guid);

    std::move(callback).Run(result);
    return;
  }
#endif  // OS_WIN
  // For in-proc case, or non-Windows platforms, we are running in the browser
  // process, so post a task to do the potentially blocking quarantine work.
  base::PostTaskAndReplyWithResult(
      GetTaskRunner().get(), FROM_HERE,
      base::BindOnce(&quarantine::QuarantineFile, full_path, source_url,
                     referrer_url, client_guid),
      std::move(callback));
}

}  // namespace quarantine
