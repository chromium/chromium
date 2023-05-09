// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>

#include "base/strings/sys_string_conversions.h"
#include "components/cronet/ios/test/cronet_test_base.h"
#include "components/cronet/ios/test/start_cronet.h"
#include "net/base/mac/url_conversions.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/test/cert_test_util.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const bool kIncludeSubdomains = true;
const bool kExcludeSubdomains = false;
const bool kSuccess = true;
const bool kError = false;
const std::string kServerCert = "quic-chain.pem";
NSDate* const kDistantFuture = [NSDate distantFuture];
}  // namespace

namespace cronet {
// Tests public-key-pinning functionality.
class PkpTest : public CronetTestBase {
 protected:
  void SetUp() override {
    CronetTestBase::SetUp();

    server_host_ =
        base::SysUTF8ToNSString(net::QuicSimpleTestServer::GetHost());
    server_domain_ =
        base::SysUTF8ToNSString(net::QuicSimpleTestServer::GetDomain());
    request_url_ =
        net::NSURLWithGURL(net::QuicSimpleTestServer::GetSimpleURL());

    // Create a Cronet enabled NSURLSession.
    NSURLSessionConfiguration* sessionConfig =
        [NSURLSessionConfiguration defaultSessionConfiguration];
    [Cronet installIntoSessionConfiguration:sessionConfig];
    url_session_ = [NSURLSession sessionWithConfiguration:sessionConfig
                                                 delegate:delegate_
                                            delegateQueue:nil];

    // Set mock cert verifier.
    [Cronet setMockCertVerifierForTesting:CreateMockCertVerifier({kServerCert},
                                                                 YES)];
  }

  void TearDown() override {
    // It is safe to call the shutdownForTesting method even if a test
    // didn't call StartCronet().
    [Cronet shutdownForTesting];
    CronetTestBase::TearDown();
  }

  // Sends a request to a given URL, waits for the response and asserts that
  // the response is either successful or containing an error depending on
  // the value of the passed |expected_success| parameter.
  void sendRequestAndAssertResult(NSURL* url, bool expected_success) {
    NSURLSessionDataTask* dataTask =
        [url_session_ dataTaskWithURL:request_url_];
    StartDataTaskAndWaitForCompletion(dataTask);
    if (expected_success) {
      ASSERT_TRUE(IsResponseSuccessful(dataTask));
    } else {
      ASSERT_FALSE(IsResponseSuccessful(dataTask));
      ASSERT_FALSE(IsResponseCanceled(dataTask));
    }
  }

  // Adds a given public-key-pin and starts a Cronet engine for testing.
  void AddPkpAndStartCronet(NSString* host,
                            NSData* hash,
                            BOOL include_subdomains,
                            NSDate* expiration_date) {
    [Cronet setEnablePublicKeyPinningBypassForLocalTrustAnchors:NO];
    NSSet* hashes = [NSSet setWithObject:hash];
    NSError* error;
    BOOL success = [Cronet addPublicKeyPinsForHost:host
                                         pinHashes:hashes
                                 includeSubdomains:include_subdomains
                                    expirationDate:(NSDate*)expiration_date
                                             error:&error];
    CHECK(success);
    CHECK(!error);
    StartCronet(net::QuicSimpleTestServer::GetPort());
  }

  // Returns an arbitrary public key hash that doesn't match with any test
  // certificate.
  static NSData* NonMatchingHash() {
    const int length = 32;
    std::string hash(length, '\077');
    return [NSData dataWithBytes:hash.c_str() length:length];
  }

  // Returns hash value that matches the hash of the public key certificate used
  // for testing.
  static NSData* MatchingHash() {
    scoped_refptr<net::X509Certificate> cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), kServerCert);
    net::HashValue hash_value;
    CalculatePublicKeySha256(*cert, &hash_value);
    CHECK_EQ(32ul, hash_value.size());
    return [NSData dataWithBytes:hash_value.data() length:hash_value.size()];
  }

  NSURLSession* url_session_;
  NSURL* request_url_;       // "https://test.example.com/simple.txt"
  NSString* server_host_;    // test.example.com
  NSString* server_domain_;  // example.com
};                           // class PkpTest

// Tests the case when a mismatching pin is set for some host that is
// different from the one the client wants to access. In that case the other
// host pinning policy should not be applied and the client is expected to
// receive the successful response with the response code 200.
TEST_F(PkpTest, TestSuccessIfPinSetForDifferentHost) {
  AddPkpAndStartCronet(@"some-other-host.com", NonMatchingHash(),
                       kExcludeSubdomains, kDistantFuture);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kSuccess));
}

// Tests the case when the pin hash does not match. The client is expected to
// receive the error response.
TEST_F(PkpTest, TestErrorIfPinDoesNotMatch) {
  AddPkpAndStartCronet(server_host_, NonMatchingHash(), kExcludeSubdomains,
                       kDistantFuture);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kError));
}

// Tests the case when the pin hash matches. The client is expected to
// receive the successful response with the response code 200.
TEST_F(PkpTest, TestSuccessIfPinMatches) {
  AddPkpAndStartCronet(server_host_, MatchingHash(), kExcludeSubdomains,
                       kDistantFuture);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kSuccess));
}

TEST_F(PkpTest, TestBypass) {
  [Cronet setEnablePublicKeyPinningBypassForLocalTrustAnchors:YES];

  NSSet* hashes = [NSSet setWithObject:NonMatchingHash()];
  NSError* error;
  BOOL success = [Cronet addPublicKeyPinsForHost:server_host_
                                       pinHashes:hashes
                               includeSubdomains:kExcludeSubdomains
                                  expirationDate:(NSDate*)kDistantFuture
                                           error:&error];

  EXPECT_FALSE(success);
  EXPECT_EQ([error code], CRNErrorUnsupportedConfig);
}

// Tests the case when the pin hash does not match and the client accesses the
// subdomain of the configured PKP host with includeSubdomains flag set to true.
// The client is expected to receive the error response.
TEST_F(PkpTest, TestIncludeSubdomainsFlagEqualTrue) {
  AddPkpAndStartCronet(server_domain_, NonMatchingHash(), kIncludeSubdomains,
                       kDistantFuture);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kError));
}

// Tests the case when the pin hash does not match and the client accesses the
// subdomain of the configured PKP host with includeSubdomains flag set to
// false. The client is expected to receive the successful response with the
// response code 200.
TEST_F(PkpTest, TestIncludeSubdomainsFlagEqualFalse) {
  AddPkpAndStartCronet(server_domain_, NonMatchingHash(), kExcludeSubdomains,
                       kDistantFuture);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kSuccess));
}

// Tests a mismatching pin that will expire in 10 seconds. The pins should be
// still valid and enforced during the request; thus returning the pin match
// error.
TEST_F(PkpTest, TestSoonExpiringPin) {
  AddPkpAndStartCronet(server_host_, NonMatchingHash(), kExcludeSubdomains,
                       [NSDate dateWithTimeIntervalSinceNow:10]);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kError));
}

// Tests mismatching pin that expired 1 second ago. Since the pin has
// expired, it should not be enforced during the request; thus a successful
// response is expected.
TEST_F(PkpTest, TestRecentlyExpiredPin) {
  AddPkpAndStartCronet(server_host_, NonMatchingHash(), kExcludeSubdomains,
                       [NSDate dateWithTimeIntervalSinceNow:-1]);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kSuccess));
}

// Tests that host pinning is not persisted between multiple CronetEngine
// instances.
TEST_F(PkpTest, TestPinsAreNotPersisted) {
  AddPkpAndStartCronet(server_host_, NonMatchingHash(), kExcludeSubdomains,
                       kDistantFuture);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kError));
  [Cronet shutdownForTesting];

  // Restart Cronet engine and try the same request again. Since the pins are
  // not persisted, a successful response is expected.
  StartCronet(net::QuicSimpleTestServer::GetPort());
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kSuccess));
}

// Tests that an error is returned when PKP hash size is not equal to 256 bits.
TEST_F(PkpTest, TestHashLengthError) {
  [Cronet setEnablePublicKeyPinningBypassForLocalTrustAnchors:NO];
  char hash[31];
  NSData* shortHash = [NSData dataWithBytes:hash length:sizeof(hash)];
  NSSet* hashes = [NSSet setWithObject:shortHash];
  NSError* error;
  BOOL success = [Cronet addPublicKeyPinsForHost:server_host_
                                       pinHashes:hashes
                               includeSubdomains:kExcludeSubdomains
                                  expirationDate:kDistantFuture
                                           error:&error];
  EXPECT_FALSE(success);
  ASSERT_TRUE(error != nil);
  EXPECT_STREQ([CRNCronetErrorDomain cStringUsingEncoding:NSUTF8StringEncoding],
               [error.domain cStringUsingEncoding:NSUTF8StringEncoding]);
  EXPECT_EQ(CRNErrorInvalidArgument, error.code);
  EXPECT_TRUE([error.description rangeOfString:@"Invalid argument"].location !=
              NSNotFound);
  EXPECT_TRUE([error.description rangeOfString:@"pinHashes"].location !=
              NSNotFound);
  EXPECT_STREQ("pinHashes", [error.userInfo[CRNInvalidArgumentKey]
                                cStringUsingEncoding:NSUTF8StringEncoding]);
}

// Tests that setting pins for the same host second time overrides the previous
// pins.
TEST_F(PkpTest, TestPkpOverrideNonMatchingToMatching) {
  [Cronet setEnablePublicKeyPinningBypassForLocalTrustAnchors:NO];
  // Add non-matching pin.
  BOOL success =
      [Cronet addPublicKeyPinsForHost:server_host_
                            pinHashes:[NSSet setWithObject:NonMatchingHash()]
                    includeSubdomains:kExcludeSubdomains
                       expirationDate:kDistantFuture
                                error:nil];
  ASSERT_TRUE(success);
  // Add matching pin.
  AddPkpAndStartCronet(server_host_, MatchingHash(), kExcludeSubdomains,
                       kDistantFuture);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kSuccess));
}

// Tests that setting pins for the same host second time overrides the previous
// pins.
TEST_F(PkpTest, TestPkpOverrideMatchingToNonMatching) {
  [Cronet setEnablePublicKeyPinningBypassForLocalTrustAnchors:NO];
  // Add matching pin.
  BOOL success =
      [Cronet addPublicKeyPinsForHost:server_host_
                            pinHashes:[NSSet setWithObject:MatchingHash()]
                    includeSubdomains:kExcludeSubdomains
                       expirationDate:kDistantFuture
                                error:nil];
  ASSERT_TRUE(success);
  // Add non-matching pin.
  AddPkpAndStartCronet(server_host_, NonMatchingHash(), kExcludeSubdomains,
                       kDistantFuture);
  ASSERT_NO_FATAL_FAILURE(sendRequestAndAssertResult(request_url_, kError));
}

}  // namespace cronet
