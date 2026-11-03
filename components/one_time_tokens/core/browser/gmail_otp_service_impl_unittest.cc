// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_service_impl.h"

#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

class GmailOtpServiceImplTest : public testing::Test {
 public:
  GmailOtpServiceImplTest() {
    service_ = std::make_unique<GmailOtpServiceImpl>(
        &backend_, &fake_affiliation_service_, std::nullopt);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeGmailOtpBackend backend_;
  affiliations::FakeAffiliationService fake_affiliation_service_;
  std::unique_ptr<GmailOtpServiceImpl> service_;
};

TEST_F(GmailOtpServiceImplTest, ExposesBackendAndLogSink) {
  EXPECT_EQ(service_->GetBackend(), &backend_);
  EXPECT_EQ(service_->GetLogSink(), backend_.GetLogSink());
}

}  // namespace one_time_tokens
