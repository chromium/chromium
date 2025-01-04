// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_VOTES_UPLOADER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_VOTES_UPLOADER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class VotesUploaderTestApi {
 public:
  explicit VotesUploaderTestApi(VotesUploader* votes_uploader)
      : votes_uploader_(*votes_uploader) {}

  base::SequencedTaskRunner& task_runner() {
    return votes_uploader_->task_runner();
  }

  // Blocks until all pending votes have been emitted. This fails if either a
  // timeout is hit or if the BrowserAutofillManager::vote_upload_task_runner_
  // has not been initialized yet.
  [[nodiscard]] testing::AssertionResult FlushPendingVotes(
      base::TimeDelta timeout = base::Seconds(10));

 private:
  raw_ref<VotesUploader> votes_uploader_;
};

inline VotesUploaderTestApi test_api(VotesUploader& votes_uploader) {
  return VotesUploaderTestApi(&votes_uploader);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_VOTES_UPLOADER_TEST_API_H_
