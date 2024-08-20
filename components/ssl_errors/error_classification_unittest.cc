// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ssl_errors/error_classification.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;

namespace {

const char kSslErrorCauseHistogram[] = "interstitial.ssl.cause.overridable";

}  // namespace

class SSLErrorClassificationTest : public ::testing::Test {
 public:
  SSLErrorClassificationTest()
      : field_trial_test_(new network_time::FieldTrialTest()) {}
  network_time::FieldTrialTest* field_trial_test() {
    return field_trial_test_.get();
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  std::unique_ptr<network_time::FieldTrialTest> field_trial_test_;
};

TEST_F(SSLErrorClassificationTest, TestNameMismatch) {
  scoped_refptr<net::X509Certificate> example_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "subjectAltName_www_example_com.pem");
  ASSERT_TRUE(example_cert);
  std::vector<std::string> dns_names_example;
  example_cert->GetSubjectAltName(&dns_names_example, nullptr);
  ASSERT_THAT(dns_names_example, ElementsAre("www.example.com"));
  std::vector<std::string> hostname_tokens_example =
      ssl_errors::Tokenize(dns_names_example[0]);
  ASSERT_THAT(hostname_tokens_example, ElementsAre("www", "example", "com"));
  std::vector<std::vector<std::string>> dns_name_tokens_example;
  dns_name_tokens_example.push_back(hostname_tokens_example);
  ASSERT_EQ(1u, dns_name_tokens_example.size());  // [["www","example","com"]]
  ASSERT_THAT(dns_name_tokens_example[0], ElementsAre("www", "example", "com"));

  {
    GURL origin("https://example.com");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_TRUE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_example, &www_host));
    EXPECT_EQ("www.example.com", www_host);
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_example));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_example,
                                               host_name_tokens));
    EXPECT_FALSE(ssl_errors::IsSubDomainOutsideWildcard(origin, *example_cert));
    EXPECT_FALSE(
        ssl_errors::IsCertLikelyFromMultiTenantHosting(origin, *example_cert));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *example_cert));
  }

  {
    GURL origin("https://foo.blah.example.com");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_example, &www_host));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_example));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_example,
                                               host_name_tokens));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *example_cert));
  }

  {
    GURL origin("https://foo.www.example.com");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_example, &www_host));
    EXPECT_TRUE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                              dns_name_tokens_example));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_example,
                                               host_name_tokens));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *example_cert));
  }

  {
    GURL origin("https://www.example.com.foo");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_example, &www_host));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_example));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_example,
                                               host_name_tokens));
    EXPECT_FALSE(ssl_errors::IsCertLikelyFromSameDomain(origin, *example_cert));
  }

  {
    GURL origin("https://www.fooexample.com.");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_example, &www_host));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_example));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_example,
                                               host_name_tokens));
    EXPECT_FALSE(ssl_errors::IsCertLikelyFromSameDomain(origin, *example_cert));
  }

  // Ensure that a certificate with no SubjectAltName does not fall back to
  // the Subject CN when evaluating hostnames.
  {
    scoped_refptr<net::X509Certificate> google_cert =
        net::X509Certificate::CreateFromBytes(google_der);
    ASSERT_TRUE(google_cert);

    GURL origin("https://google.com");

    base::HistogramTester histograms;
    ssl_errors::RecordUMAStatistics(true, base::Time::NowFromSystemTime(),
                                    origin, net::ERR_CERT_COMMON_NAME_INVALID,
                                    *google_cert);

    // Verify that we recorded only NO_SUBJECT_ALT_NAME and no other causes.
    histograms.ExpectUniqueSample(kSslErrorCauseHistogram,
                                  ssl_errors::NO_SUBJECT_ALT_NAME, 1);
  }

  {
    scoped_refptr<net::X509Certificate> webkit_cert =
        net::X509Certificate::CreateFromBytes(webkit_der);
    ASSERT_TRUE(webkit_cert);
    std::vector<std::string> dns_names_webkit;
    webkit_cert->GetSubjectAltName(&dns_names_webkit, nullptr);
    ASSERT_THAT(dns_names_webkit, ElementsAre("*.webkit.org", "webkit.org"));
    std::vector<std::string> hostname_tokens_webkit_0 =
        ssl_errors::Tokenize(dns_names_webkit[0]);
    ASSERT_THAT(hostname_tokens_webkit_0, ElementsAre("*", "webkit", "org"));
    std::vector<std::string> hostname_tokens_webkit_1 =
        ssl_errors::Tokenize(dns_names_webkit[1]);
    ASSERT_THAT(hostname_tokens_webkit_1, ElementsAre("webkit", "org"));
    std::vector<std::vector<std::string>> dns_name_tokens_webkit;
    dns_name_tokens_webkit.push_back(hostname_tokens_webkit_0);
    dns_name_tokens_webkit.push_back(hostname_tokens_webkit_1);
    ASSERT_EQ(2u, dns_name_tokens_webkit.size());
    GURL origin("https://a.b.webkit.org");
    std::string www_host;
    std::vector<std::string> host_name_tokens = base::SplitString(
        origin.host(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_FALSE(
        ssl_errors::GetWWWSubDomainMatch(origin, dns_names_webkit, &www_host));
    EXPECT_FALSE(ssl_errors::NameUnderAnyNames(host_name_tokens,
                                               dns_name_tokens_webkit));
    EXPECT_FALSE(ssl_errors::AnyNamesUnderName(dns_name_tokens_webkit,
                                               host_name_tokens));
    EXPECT_TRUE(ssl_errors::IsSubDomainOutsideWildcard(origin, *webkit_cert));
    EXPECT_FALSE(
        ssl_errors::IsCertLikelyFromMultiTenantHosting(origin, *webkit_cert));
    EXPECT_TRUE(ssl_errors::IsCertLikelyFromSameDomain(origin, *webkit_cert));
  }
}

TEST_F(SSLErrorClassificationTest, TestHostNameHasKnownTLD) {
  EXPECT_TRUE(ssl_errors::HostNameHasKnownTLD("www.google.com"));
  EXPECT_TRUE(ssl_errors::HostNameHasKnownTLD("b.appspot.com"));
  EXPECT_FALSE(ssl_errors::HostNameHasKnownTLD("a.private"));
}

TEST_F(SSLErrorClassificationTest, TestPrivateURL) {
  EXPECT_FALSE(ssl_errors::IsHostnameNonUniqueOrDotless("www.foogoogle.com."));
  EXPECT_TRUE(ssl_errors::IsHostnameNonUniqueOrDotless("go"));
  EXPECT_TRUE(ssl_errors::IsHostnameNonUniqueOrDotless("172.17.108.108"));
  EXPECT_TRUE(ssl_errors::IsHostnameNonUniqueOrDotless("foo.blah"));
}

TEST_F(SSLErrorClassificationTest, GetClockState) {
  // This test aims to obtain all possible return values of
  // |GetClockState|.
  TestingPrefServiceSimple pref_service;
  network_time::NetworkTimeTracker::RegisterPrefs(pref_service.registry());
  network_time::NetworkTimeTracker network_time_tracker(
      std::make_unique<base::DefaultClock>(),
      std::make_unique<base::DefaultTickClock>(), &pref_service,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      std::nullopt);

  ssl_errors::SetBuildTimeForTesting(base::Time::Now());
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_UNKNOWN,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  ssl_errors::SetBuildTimeForTesting(base::Time::Now() - base::Days(367));
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_FUTURE,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  ssl_errors::SetBuildTimeForTesting(base::Time::Now() + base::Days(3));
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_PAST,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  // Intentionally leave the build time alone.  It should be ignored
  // in favor of network time.
  network_time_tracker.UpdateNetworkTime(
      base::Time::Now() + base::Hours(1),
      base::Seconds(1),         // resolution
      base::Milliseconds(250),  // latency
      base::TimeTicks::Now());  // posting time
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_PAST,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  network_time_tracker.UpdateNetworkTime(
      base::Time::Now() - base::Hours(1),
      base::Seconds(1),         // resolution
      base::Milliseconds(250),  // latency
      base::TimeTicks::Now());  // posting time
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_FUTURE,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  network_time_tracker.UpdateNetworkTime(
      base::Time::Now(),
      base::Seconds(1),         // resolution
      base::Milliseconds(250),  // latency
      base::TimeTicks::Now());  // posting time
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_OK,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  // Now clear the network time.  The build time should reassert
  // itself.
  network_time_tracker.ClearNetworkTimeForTesting();
  ssl_errors::SetBuildTimeForTesting(base::Time::Now() + base::Days(3));
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_PAST,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));

  // Now set the build time to something reasonable.  We should be
  // back to the know-nothing state.
  ssl_errors::SetBuildTimeForTesting(base::Time::Now());
  EXPECT_EQ(
      ssl_errors::ClockState::CLOCK_STATE_UNKNOWN,
      ssl_errors::GetClockState(base::Time::Now(), &network_time_tracker));
}
