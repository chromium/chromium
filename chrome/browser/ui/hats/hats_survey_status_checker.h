// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SURVEY_STATUS_CHECKER_H_
#define CHROME_BROWSER_UI_HATS_HATS_SURVEY_STATUS_CHECKER_H_

#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/simple_url_loader.h"

class HatsSurveyStatusChecker : public ProfileObserver {
 public:
  enum class Status {
    kSuccess,
    kUnreachable,
    kResponseHeaderError,
    kOverCapacity
  };

  static constexpr char kHatsSurveyDataPath[] =
      "insights/consumersurveys/gk/prompt?site=";
  static constexpr int kTimeoutSecs = 3;

  // HaTS response header that indicates the survey is full.
  static constexpr char kReasonHeader[] = "X-Why";
  static constexpr char kReasonOverCapacity[] = "Exhausted";

  explicit HatsSurveyStatusChecker(Profile* profile);
  HatsSurveyStatusChecker(const HatsSurveyStatusChecker&) = delete;
  ~HatsSurveyStatusChecker() override;

  HatsSurveyStatusChecker& operator=(const HatsSurveyStatusChecker&) = delete;

  // Fetches the survey from server and checks the response headers.
  // Calls |on_success| if the response shows the survey is under
  // capacity; calls |on_failure| if the request fails or shows the survey is
  // over capacity.
  virtual void CheckSurveyStatus(const std::string& site_id,
                                 base::OnceClosure on_success,
                                 base::OnceCallback<void(Status)> on_failure);

  base::OnceClosure CreateTimeoutCallbackForTesting();

 protected:
  // Used for testing only.
  HatsSurveyStatusChecker();

  // Overridden only by tests.
  virtual std::string HatsSurveyURLWithoutId();
  virtual int SurveyCheckTimeoutSecs();

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Callbacks for survey capacity checking.
  void OnURLLoadComplete(scoped_refptr<net::HttpResponseHeaders> headers);
  void OnTimeout();

  content::StoragePartition* GetStoragePartition() const;

  // The off the record profile used for fetching survey status.
  Profile* otr_profile_ = nullptr;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::OnceClosure on_success_;
  base::OnceCallback<void(Status)> on_failure_;
  base::OneShotTimer request_timer_;
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SURVEY_STATUS_CHECKER_H_
