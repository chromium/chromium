// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_requirements_service.h"

#include <map>

#include "base/logging.h"
#include "base/test/bind_test_util.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
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
    auto iter = data_to_return_.find(origin);
    std::move(callback).Run(iter != data_to_return_.end()
                                ? iter->second
                                : autofill::PasswordRequirementsSpec());
  }

  void SetDataToReturn(const GURL& origin,
                       const autofill::PasswordRequirementsSpec& spec) {
    data_to_return_[origin] = spec;
  }

 private:
  std::map<GURL, autofill::PasswordRequirementsSpec> data_to_return_;
};

class PasswordRequirementsServiceTest : public testing::Test {
 public:
  PasswordRequirementsServiceTest()
      : test_origin_("http://www.example.com"),
        // Ownership is passed to the |service_| below.
        fetcher_ptr_(new MockPasswordRequirementsSpecFetcher()),
        service_(std::unique_ptr<MockPasswordRequirementsSpecFetcher>(
            fetcher_ptr_)) {}
  ~PasswordRequirementsServiceTest() override = default;

 protected:
  // Prepopulated test data.
  GURL test_origin_;
  autofill::FormSignature test_form_signature_ = 123;
  autofill::FieldSignature test_field_signature_ = 22;

  // Weak pointer.
  MockPasswordRequirementsSpecFetcher* fetcher_ptr_;
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
    autofill::PasswordRequirementsSpec* spec_for_signature = nullptr;
    autofill::PasswordRequirementsSpec* spec_for_domain = nullptr;
    autofill::PasswordRequirementsSpec* expected;
  } tests[] = {
      {
          .test_name = "No data prefechted", .expected = &spec_l0_p0,
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

}  // namespace

}  // namespace password_manager
