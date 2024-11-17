// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_breakage_exception.h"

#include <memory>
#include <string_view>

#include "base/strings/strcat.h"
#include "components/fingerprinting_protection_filter/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fingerprinting_protection_filter {
namespace {

class FingerprintingProtectionBreakageExceptionTest : public ::testing::Test {
 public:
  FingerprintingProtectionBreakageExceptionTest() = default;
  void SetUp() override { InitializePrefService(); }
  void TearDown() override {}

  void InitializePrefService() {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kRefreshHeuristicBreakageException);
  }
  // Checks variants of a URL to test that they are all excepted.
  void ExpectAllUrlVariantsHaveException(std::string_view domain) {
    // http
    EXPECT_TRUE(HasBreakageException(GURL(base::StrCat({"http://", domain})),
                                     *pref_service_));
    // https
    EXPECT_TRUE(HasBreakageException(GURL(base::StrCat({"https://", domain})),
                                     *pref_service_));
    // http with subdomain
    EXPECT_TRUE(HasBreakageException(
        GURL(base::StrCat({"http://subdomain.", domain})), *pref_service_));
    // https with subdomain
    EXPECT_TRUE(HasBreakageException(
        GURL(base::StrCat({"https://subdomain.", domain})), *pref_service_));
    // http with file path
    EXPECT_TRUE(HasBreakageException(
        GURL(base::StrCat({"http://", domain, "/main.html"})), *pref_service_));
    // https with file path
    EXPECT_TRUE(HasBreakageException(
        GURL(base::StrCat({"https://", domain, "/main.html"})),
        *pref_service_));
    // http with subdomain and file path
    EXPECT_TRUE(HasBreakageException(
        GURL(base::StrCat({"http://my_subdomain.", domain, "/main.html"})),
        *pref_service_));
    // https with subdomain and file path
    EXPECT_TRUE(HasBreakageException(
        GURL(base::StrCat({"https://my_subdomain.", domain, "/main.html"})),
        *pref_service_));
  }

 protected:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(FingerprintingProtectionBreakageExceptionTest,
       BreakageExceptionsSuccessfullyQueriedEverywhereOnSite) {
  // Initially we don't have any exception saved
  EXPECT_FALSE(HasBreakageException(GURL("http://foo.com"), *pref_service_));
  // Add exception based on full URL
  EXPECT_TRUE(AddBreakageException(GURL("http://foo.com"), *pref_service_));
  ExpectAllUrlVariantsHaveException("foo.com");

  // Unknown site still has no exception
  EXPECT_FALSE(HasBreakageException(GURL("http://bazbaz.com"), *pref_service_));
  // Add exception based on subdomain
  EXPECT_TRUE(AddBreakageException(GURL("http://subdomain.bazbaz.com"),
                                   *pref_service_));
  ExpectAllUrlVariantsHaveException("bazbaz.com");
}

TEST_F(FingerprintingProtectionBreakageExceptionTest,
       BreakageExceptionCorrectForPrivateRegistry) {
  // Add subdomain of .appspot.com, which is a private registry, so appspot.com
  // shouldn't be excepted, but rather specifically
  // excepted_subdomain.appspot.com.
  EXPECT_TRUE(AddBreakageException(
      GURL("http://excepted_subdomain.appspot.com"), *pref_service_));
  // Subdomain should be excepted correctly.
  ExpectAllUrlVariantsHaveException("excepted_subdomain.appspot.com");
  // Another subdomain of appspot.com shouldn't be excepted.
  EXPECT_FALSE(HasBreakageException(
      GURL("http://another_nonexcepted_subdomain.appspot.com"),
      *pref_service_));
}

TEST_F(FingerprintingProtectionBreakageExceptionTest,
       ReturnsFalseWithInvalidURL) {
  EXPECT_FALSE(AddBreakageException(GURL("http://invalid_url/file.html"),
                                    *pref_service_));
  EXPECT_FALSE(HasBreakageException(GURL("http://invalid_url/file.html"),
                                    *pref_service_));
}

}  // namespace
}  // namespace fingerprinting_protection_filter
