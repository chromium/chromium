// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_TASK_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_TASK_H_

#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_data_fetcher.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "components/webapps/browser/installable/installable_page_data.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/installable/installable_task.h"

namespace content {
class WebContents;
}  // namespace content

namespace webapps {

class InstallableManager;

// This class is responsible for processing the installable check. It triggers
// the InstallableFetcher and InstallableEvaluator according to the
// InstallableParams.
class InstallableTask {
 public:
  InstallableTask(content::WebContents* web_contents,
                  base::WeakPtr<InstallableManager> installable_manager,
                  const InstallableParams& params,
                  InstallableCallback callback,
                  InstallablePageData& page_data);

  InstallableTask(const InstallableParams params,
                  InstallablePageData& page_data);
  ~InstallableTask();

  InstallableTask(const InstallableTask&) = delete;
  InstallableTask& operator=(const InstallableTask&) = delete;

  void Start();

  const InstallableParams params() const { return params_; }
  void RunCallback();
  void ResetWithError(InstallableStatusCode code);

 private:
  void IncrementStateAndWorkOnNextTask();
  void OnFetchedData(InstallableStatusCode code);

  // Evaluater.
  void CheckEligibility();
  void CheckInstallability();

  base::WeakPtr<content::WebContents> web_contents_;
  base::WeakPtr<InstallableManager> manager_;

  InstallableParams params_;
  InstallableCallback callback_;

  const raw_ref<InstallablePageData> page_data_;
  std::unique_ptr<InstallableDataFetcher> fetcher_;
  std::unique_ptr<InstallableEvaluator> evaluator_;

  enum State {
    kInactive = 0,
    kCheckEligibility = 1,
    kFetchWebPageMetadata = 2,
    kFetchManifest = 3,
    kCheckInstallability = 4,
    kFetchPrimaryIcon = 5,
    kFetchScreenshots = 6,
    kComplete = 7,
    kMaxState
  };
  // The current running evaluation state. The order of the |State| enum above
  // is used for determining how we progress through the evaluation (i.e. it
  // decides the order of the evaluation steps).
  int state_ = kInactive;

  std::vector<InstallableStatusCode> errors_;
  bool installability_check_passed_ = false;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_TASK_H_
