// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_LOOKUP_MECHANISM_RUNNER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_LOOKUP_MECHANISM_RUNNER_H_

#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"

namespace safe_browsing {

// This class is responsible for handling timeouts for running a specific Safe
// Browsing mechanism lookup. It keeps a timer while the mechanism runs and
// responds back to the caller either with the mechanism's results or with a
// flag indicating that the check timed out. If the check timed out, this object
// will also perform clean-up by destroying the mechanism object.
class SafeBrowsingLookupMechanismRunner {
 public:
  using CompleteCheckCallbackWithTimeout = base::OnceCallback<void(
      bool timed_out,
      std::optional<std::unique_ptr<
          SafeBrowsingLookupMechanism::CompleteCheckResult>> result)>;

  // |lookup_mechanism| is the mechanism that the runner will use to perform the
  // lookup. |complete_check_callback| is the callback that will be called if
  // the run does not complete synchronously; its |result| parameter will only
  // be populated if the run did not time out. |performed_check_suffix| is a
  // suffix used for a metric logging how long the check took to run.
  SafeBrowsingLookupMechanismRunner(
      std::unique_ptr<SafeBrowsingLookupMechanism> lookup_mechanism,
      const std::string& performed_check_suffix,
      CompleteCheckCallbackWithTimeout complete_check_callback);
  ~SafeBrowsingLookupMechanismRunner();
  SafeBrowsingLookupMechanismRunner(const SafeBrowsingLookupMechanismRunner&) =
      delete;
  SafeBrowsingLookupMechanismRunner& operator=(
      const SafeBrowsingLookupMechanismRunner&) = delete;

  // Kicks off the lookup mechanism run and starts the timer. The result
  // includes metadata as well as a boolean indicating whether the run completed
  // synchronously and was found to be safe. In that case, the callback passed
  // through the constructor will not be called.
  SafeBrowsingLookupMechanism::StartCheckResult Run();

 private:
  // The function that the lookup mechanism calls into when its run completes.
  // This calls the |complete_check_callback_| passed in through the
  // constructor.
  void OnCompleteCheckResult(
      std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result);
  // The function that |timer_| calls when the lookup mechanism run times out.
  // This calls the |complete_check_callback_| passed in through the
  // constructor.
  void OnTimeout();
  // This performs clean up once a run is complete. It is only called once.
  void OnCheckComplete();

  // The lookup mechanism responsible for running the check and returning the
  // relevant results.
  std::unique_ptr<SafeBrowsingLookupMechanism> lookup_mechanism_;
  // Suffix used for a metric logging how long the check took to run
  // (SafeBrowsing.CheckUrl.TimeTaken.*).
  std::string performed_check_suffix_;
  // The callback passed in through the constructor that should be called either
  // when the mechanism completes or when it times out, unless the run completes
  // synchronously. Its |result| parameter will only be populated if the run did
  // not time out.
  CompleteCheckCallbackWithTimeout complete_check_callback_;
  // Timer to abort the SafeBrowsing check if it takes too long.
  std::unique_ptr<base::OneShotTimer> timer_ =
      std::make_unique<base::OneShotTimer>();
  // The time the run began. Used for metrics only.
  base::TimeTicks start_lookup_time_;

#if DCHECK_IS_ON()
  // Used only for a DCHECK to confirm that |OnCheckComplete| is called only
  // once.
  bool is_check_complete_ = false;
#endif

  base::WeakPtrFactory<SafeBrowsingLookupMechanismRunner> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_LOOKUP_MECHANISM_RUNNER_H_
