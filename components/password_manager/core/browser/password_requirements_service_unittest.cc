// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_requirements_service.h"

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/generation/password_requirements_spec_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

class MockPasswordRequirementsSpecFetcher
    : public autofill::PasswordRequirementsSpecFetcher {
 public:
  MockPasswordRequirementsSpecFetcher() = default;
  ~MockPasswordRequirementsSpecFetcher() override = default;

  void Fetch(GURL origin, FetchCallback callback) override {
    fetch_count_[origin]++;
    auto iter = data_to_return_.find(origin);
    std::move(callback).Run(iter != data_to_return_.end()
                                ? iter->second
                                : autofill::PasswordRequirementsSpec());
  }

  void SetDataToReturn(const GURL& origin,
                       const autofill::PasswordRequirementsSpec& spec) {
    data_to_return_[origin] = spec;
  }

  int GetFetchCount(const GURL& origin) const {
    auto it = fetch_count_.find(origin);
    return it != fetch_count_.end() ? it->second : 0;
  }

 private:
  std::map<GURL, autofill::PasswordRequirementsSpec> data_to_return_;
  std::map<GURL, int> fetch_count_;
};

class PasswordRequirementsServiceTest : public testing::Test {
 public:
  PasswordRequirementsServiceTest()
      : test_origin_("http://www.example.com"),
        // Ownership is passed to the |service_| below.
        fetcher_ptr_(new MockPasswordRequirementsSpecFetcher()),
        service_(std::unique_ptr<MockPasswordRequirementsSpecFetcher>(
            fetcher_ptr_)) {}

  ~PasswordRequirementsServiceTest() override {
    // This is set to `nullptr` explicitly to a) avoid that `fetcher_ptr_`
    // dangles during construction and b) keep the construction order as is.
    fetcher_ptr_ = nullptr;
  }

 protected:
  // Prepopulated test data.
  GURL test_origin_;
  autofill::FormSignature test_form_signature_{123};
  autofill::FieldSignature test_field_signature_{22};

  // Raw pointer, object is owned by `service_`.
  raw_ptr<MockPasswordRequirementsSpecFetcher> fetcher_ptr_;
  PasswordRequirementsService service_;
};

TEST_F(PasswordRequirementsServiceTest, ExerciseEverything) {
  // The following specs are names according to the following scheme:
  // spec_l${max_length value}_p${priority value}
  // values of 0 imply that no value is specified.
  // It would be possible to test the behavior with fewer instances than below
  // but these are chosen to be representative of what we expect the server
  // to send with regards to priorities.
  autofill::PasswordRequirementsSpec spec_l0_p0;  // empty spec.
  autofill::PasswordRequirementsSpec spec_l7_p0;
  spec_l7_p0.set_max_length(7u);
  autofill::PasswordRequirementsSpec spec_l8_p10;
  spec_l8_p10.set_max_length(8u);
  spec_l8_p10.set_priority(10);
  autofill::PasswordRequirementsSpec spec_l9_p20;
  spec_l9_p20.set_max_length(9u);
  spec_l9_p20.set_priority(20);
  autofill::PasswordRequirementsSpec spec_l10_p30;
  spec_l10_p30.set_max_length(10u);
  spec_l10_p30.set_priority(30);

  struct {
    const char* test_name;
    raw_ptr<autofill::PasswordRequirementsSpec> spec_for_signature = nullptr;
    raw_ptr<autofill::PasswordRequirementsSpec> spec_for_domain = nullptr;
    raw_ptr<autofill::PasswordRequirementsSpec> expected;
  } tests[] = {
      {
          .test_name = "No data prefechted",
          .expected = &spec_l0_p0,
      },
      {
          .test_name = "Only domain wide spec",
          .spec_for_domain = &spec_l7_p0,
          .expected = &spec_l7_p0,
      },
      {
          .test_name = "Only signature based spec",
          .spec_for_signature = &spec_l7_p0,
          .expected = &spec_l7_p0,
      },
      {
          .test_name = "Domain spec can override spec based on signature",
          .spec_for_signature = &spec_l8_p10,
          .spec_for_domain = &spec_l9_p20,
          .expected = &spec_l9_p20,  // priority 20 trumps priority 10.
      },
      {
          .test_name = "Signature spec can override spec based on domain",
          .spec_for_signature = &spec_l10_p30,
          .spec_for_domain = &spec_l9_p20,
          .expected = &spec_l10_p30,  // priority 30 trumps priority 20.
      },
      {
          .test_name = "Dealing with unset priority in domain",
          .spec_for_signature = &spec_l8_p10,
          .spec_for_domain = &spec_l7_p0,  // No prioritiy specified.
          .expected = &spec_l8_p10,
      },
      {
          .test_name = "Dealing with unset priority in signature",
          .spec_for_signature = &spec_l7_p0,  // No prioritiy specified.
          .spec_for_domain = &spec_l8_p10,
          .expected = &spec_l8_p10,
      },
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.test_name);

    service_.ClearDataForTesting();

    // Populate the service with data.
    if (test.spec_for_domain) {
      fetcher_ptr_->SetDataToReturn(test_origin_, *(test.spec_for_domain));
      service_.PrefetchSpec(test_origin_);
    }
    if (test.spec_for_signature) {
      service_.AddSpec(test_origin_, test_form_signature_,
                       test_field_signature_, *(test.spec_for_signature));
    }

    // Perform lookup.
    auto result = service_.GetSpec(test_origin_, test_form_signature_,
                                   test_field_signature_);

    // Validate answer.
    EXPECT_EQ(test.expected->has_priority(), result.has_priority());
    if (test.expected->has_priority()) {
      EXPECT_EQ(test.expected->priority(), result.priority());
    }

    EXPECT_EQ(test.expected->has_max_length(), result.has_max_length());
    if (test.expected->has_max_length()) {
      EXPECT_EQ(test.expected->max_length(), result.max_length());
    }
  }
}

// Tests that fetching `PasswordRequirementsSpec` for the same domain multiple
// times only results in a single network request due to caching.
TEST_F(PasswordRequirementsServiceTest, FetchPasswordRequirementsSpecCaching) {
  // Initially, no fetch should have been called.
  ASSERT_EQ(fetcher_ptr_->GetFetchCount(test_origin_), 0);
  base::test::TestFuture<autofill::PasswordRequirementsSpec> completion_future1;

  service_.FetchPasswordRequirementsSpec(test_origin_,
                                         completion_future1.GetCallback());
  ASSERT_TRUE(completion_future1.Wait());
  // Checks that password requirement was fetched once.
  EXPECT_EQ(fetcher_ptr_->GetFetchCount(test_origin_), 1);

  base::test::TestFuture<autofill::PasswordRequirementsSpec> completion_future2;

  service_.FetchPasswordRequirementsSpec(test_origin_,
                                         completion_future2.GetCallback());
  ASSERT_TRUE(completion_future2.Wait());
  EXPECT_EQ(fetcher_ptr_->GetFetchCount(test_origin_), 1);

  // We did not fetch the server a second time since `test_origin_` was already
  // in `specs_for_domains_`.
  EXPECT_EQ(fetcher_ptr_->GetFetchCount(test_origin_), 1);
}

// Tests that calling FetchPasswordRequirementsSpec with an empty domain
// completes and returns the default spec without attempting a fetch.
TEST_F(PasswordRequirementsServiceTest, FetchWithEmptyDomain) {
  GURL empty_domain;
  ASSERT_FALSE(empty_domain.is_valid());

  // Initially, no fetch should have been called.
  ASSERT_EQ(fetcher_ptr_->GetFetchCount(empty_domain), 0);

  base::test::TestFuture<autofill::PasswordRequirementsSpec> completion_future;
  service_.FetchPasswordRequirementsSpec(empty_domain,
                                         completion_future.GetCallback());
  ASSERT_TRUE(completion_future.Wait());

  // The fetcher should not be called.
  EXPECT_EQ(fetcher_ptr_->GetFetchCount(empty_domain), 0);

  // The callback should receive a empty spec.
  const auto& spec = completion_future.Get();
  EXPECT_FALSE(spec.has_min_length());
  EXPECT_FALSE(spec.has_max_length());
  EXPECT_FALSE(spec.has_priority());
}
}  // namespace

}  // namespace password_manager
