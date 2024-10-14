// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_GET_ISOLATED_WEB_APP_SIZE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_GET_ISOLATED_WEB_APP_SIZE_JOB_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "url/origin.h"

class Profile;

namespace web_app {

class WithAppResources;

class GetIsolatedWebAppSizeJob {
 public:
  using ResultCallback =
      base::OnceCallback<void(CommandResult,
                              base::flat_map<url::Origin, int64_t>)>;

  GetIsolatedWebAppSizeJob(Profile* profile,
                           base::Value::Dict& debug_value,
                           ResultCallback result_callback);
  ~GetIsolatedWebAppSizeJob();

  void Start(WithAppResources* lock_with_app_resources);

 private:
  void StoragePartitionSizeFetched(const url::Origin& iwa_origin, int64_t size);
  void MaybeCompleteCommand();

  int pending_task_count_ = 0;
  base::flat_map<url::Origin, int64_t> browsing_data_;
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<WithAppResources> lock_with_app_resources_ = nullptr;
  const raw_ref<base::Value::Dict> debug_value_;
  ResultCallback result_callback_;

  base::WeakPtrFactory<GetIsolatedWebAppSizeJob> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_GET_ISOLATED_WEB_APP_SIZE_JOB_H_
