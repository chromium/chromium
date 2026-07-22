// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/certificate_signals_collector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/certificate_matching/certificate_principal_pattern.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/keypair.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace device_signals {

namespace {

constexpr char CN_KEY[] = "CN";
constexpr char CHALLENGE[] = "challenge";

base::DictValue CreateFilter(const std::string cn) {
  base::DictValue filter;
  filter.Set(CN_KEY, cn);
  return filter;
}

certificate_matching::CertificatePrincipalPattern GetCertPattern(
    const base::DictValue& dict) {
  return certificate_matching::CertificatePrincipalPattern::
      ParseFromOptionalDict(&dict, /*key_common_name=*/CN_KEY,
                            /*key_locality=*/"", /*key_organization=*/"",
                            /*key_organization_unit=*/"");
}

GetCertificateOptions CreateCertificateOptions(const base::DictValue& subject,
                                               const base::DictValue& issuer) {
  GetCertificateOptions options;
  options.subject_pattern = GetCertPattern(subject);
  options.issuer_pattern = GetCertPattern(issuer);
  options.challenge = CHALLENGE;
  return options;
}

class FakeSSLPrivateKey : public net::SSLPrivateKey {
 public:
  FakeSSLPrivateKey(bool simulate_sign_failure,
                    std::vector<uint16_t> algorithm_preferences)
      : simulate_sign_failure_(simulate_sign_failure),
        algorithm_preferences_(std::move(algorithm_preferences)) {}

  std::string GetProviderName() override { return "FakeProvider"; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return algorithm_preferences_;
  }

  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override {
    if (simulate_sign_failure_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), net::ERR_FAILED,
                                    std::vector<uint8_t>()));
      return;
    }
    std::vector<uint8_t> signature(input.begin(), input.end());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), net::OK, signature));
  }

 private:
  ~FakeSSLPrivateKey() override = default;
  bool simulate_sign_failure_ = false;
  std::vector<uint16_t> algorithm_preferences_;
};

class FakeClientCertIdentity : public net::ClientCertIdentity {
 public:
  FakeClientCertIdentity(scoped_refptr<net::X509Certificate> cert,
                         bool simulate_key_missing,
                         bool simulate_sign_failure,
                         std::vector<uint16_t> algorithm_preferences)
      : net::ClientCertIdentity(std::move(cert)),
        simulate_key_missing_(simulate_key_missing),
        simulate_sign_failure_(simulate_sign_failure),
        algorithm_preferences_(std::move(algorithm_preferences)) {}
  ~FakeClientCertIdentity() override = default;

  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback) override {
    scoped_refptr<FakeSSLPrivateKey> key =
        simulate_key_missing_
            ? nullptr
            : base::MakeRefCounted<FakeSSLPrivateKey>(simulate_sign_failure_,
                                                      algorithm_preferences_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(private_key_callback), key));
  }

 private:
  bool simulate_key_missing_ = false;
  bool simulate_sign_failure_ = false;
  std::vector<uint16_t> algorithm_preferences_;
};

class FakeClientCertStore : public net::ClientCertStore {
 public:
  explicit FakeClientCertStore(
      const std::vector<std::string>& common_names,
      std::vector<base::Time> start_times = {},
      bool simulate_key_missing = false,
      bool simulate_sign_failure = false,
      bool simulate_corrupt_cert = false,
      std::vector<uint16_t> algorithm_preferences = {0x0401})
      : common_names_(common_names),
        start_times_(start_times),
        simulate_key_missing_(simulate_key_missing),
        simulate_sign_failure_(simulate_sign_failure),
        simulate_corrupt_cert_(simulate_corrupt_cert),
        algorithm_preferences_(std::move(algorithm_preferences)) {}
  ~FakeClientCertStore() override = default;

  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    net::ClientCertIdentityList identities;
    for (size_t i = 0; i < common_names_.size(); ++i) {
      const auto& cn = common_names_[i];
      std::vector<uint8_t> cert_der;
      if (i < start_times_.size()) {
        const uint32_t kSerial = 1;
        base::Time not_valid_before = start_times_[i];
        base::Time not_valid_after = not_valid_before + base::Hours(1);
        auto key = crypto::keypair::PrivateKey::GenerateEcP256();
        std::string der_cert;
        CHECK(net::x509_util::CreateSelfSignedCert(
            key.key(), net::x509_util::DIGEST_SHA256, cn, kSerial,
            not_valid_before, not_valid_after, {}, &der_cert));
        cert_der = base::ToVector(base::as_byte_span(der_cert));
      } else {
        cert_der = net::x509_util::CreateUnusableCert(cn);
      }
      scoped_refptr<net::X509Certificate> cert =
          simulate_corrupt_cert_
              ? nullptr
              : net::X509Certificate::CreateFromBytes(cert_der);
      if (cert || simulate_corrupt_cert_) {
        identities.push_back(std::make_unique<FakeClientCertIdentity>(
            std::move(cert), simulate_key_missing_, simulate_sign_failure_,
            algorithm_preferences_));
      }
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(identities)));
  }

 private:
  std::vector<std::string> common_names_;
  std::vector<base::Time> start_times_;
  bool simulate_key_missing_ = false;
  bool simulate_sign_failure_ = false;
  bool simulate_corrupt_cert_ = false;
  std::vector<uint16_t> algorithm_preferences_;
};

}  // namespace

class CertificateSignalsCollectorTest : public testing::Test {
 protected:
  CertificateSignalsCollectorTest() = default;

  std::unique_ptr<net::ClientCertStore> CreateFakeClientCertStore() {
    if (simulate_null_store_) {
      return nullptr;
    }
    return std::make_unique<FakeClientCertStore>(
        cn_to_return_, cert_start_times_, simulate_key_missing_,
        simulate_sign_failure_, simulate_corrupt_cert_, algorithm_preferences_);
  }

  // Helper to pre-populate exactly how many certs (and with what common name)
  // the FakeClientCertStore should retrieve.
  void SetCertificatesToReturn(size_t count, const std::string& cn) {
    cn_to_return_.clear();
    for (size_t i = 0; i < count; ++i) {
      cn_to_return_.push_back(cn);
    }
  }

  void VerifySuccessResponse(const SignalsAggregationResponse& response,
                             size_t expected_count,
                             const std::string& cert_cn,
                             bool expected_truncated = false) {
    ASSERT_TRUE(response.certificate_signals_response.has_value());
    EXPECT_FALSE(
        response.certificate_signals_response->collection_error.has_value());
    EXPECT_EQ(response.certificate_signals_response->truncated_certificates,
              expected_truncated);
    EXPECT_EQ(
        response.certificate_signals_response->serialized_caa_responses.size(),
        expected_count);
    histogram_tester_.ExpectUniqueSample(
        "Enterprise.DeviceSignals.Collection.Success",
        SignalName::kCertificates, 1);
    histogram_tester_.ExpectUniqueSample(
        "Enterprise.DeviceSignals.Collection.Success.Certificates.Items",
        expected_count, 1);
    histogram_tester_.ExpectTotalCount(
        "Enterprise.DeviceSignals.Collection.Success.Certificates.Latency", 1);

    for (size_t i = 0; i < expected_count; ++i) {
      const std::string& serialized_response =
          response.certificate_signals_response->serialized_caa_responses[i];

      // Validating the certificate signature and context binding.
      enterprise_management::SignedCertificateDetails signed_details;
      ASSERT_TRUE(signed_details.ParseFromString(serialized_response));
      EXPECT_FALSE(signed_details.signature().empty());

      std::string expected_prefix = "SecuritySignalsReportCertificate";
      std::string expected_signature = expected_prefix + signed_details.data();
      EXPECT_EQ(signed_details.signature(), expected_signature);

      // Validating the CertificateDetails.
      enterprise_management::CertificateDetails inner_cert_details;
      ASSERT_TRUE(inner_cert_details.ParseFromString(signed_details.data()));
      EXPECT_EQ(inner_cert_details.challenge(), CHALLENGE);
      EXPECT_EQ(inner_cert_details.algorithm(),
                enterprise_management::
                    CertificateDetails_SignatureAlgorithm_RSA_PKCS1_SHA256);
      EXPECT_FALSE(inner_cert_details.certificate().empty());

      // Validate the certificate itself.
      const std::string& cert_bytes_str = inner_cert_details.certificate();
      scoped_refptr<net::X509Certificate> parsed_cert =
          net::X509Certificate::CreateFromBytes(
              base::as_byte_span(cert_bytes_str));
      ASSERT_TRUE(parsed_cert);
      EXPECT_EQ(parsed_cert->subject().common_name, cert_cn);
    }
  }

  void VerifyFailureResponse(const SignalsAggregationResponse& response,
                             SignalCollectionError expected_error) {
    ASSERT_TRUE(response.certificate_signals_response.has_value());
    EXPECT_TRUE(
        response.certificate_signals_response->collection_error.has_value());
    EXPECT_EQ(response.certificate_signals_response->collection_error.value(),
              expected_error);
    histogram_tester_.ExpectUniqueSample(
        "Enterprise.DeviceSignals.Collection.Failure",
        SignalName::kCertificates, 1);
    histogram_tester_.ExpectUniqueSample(
        "Enterprise.DeviceSignals.Collection.Failure.Certificates."
        "CollectionLevelError",
        expected_error, 1);
    histogram_tester_.ExpectTotalCount(
        "Enterprise.DeviceSignals.Collection.Failure.Certificates.Latency", 1);
  }

  void InitializeCollector() {
    collector_ = std::make_unique<CertificateSignalsCollector>(
        CreateFakeClientCertStore());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::vector<std::string> cn_to_return_;
  std::vector<base::Time> cert_start_times_;
  bool simulate_null_store_ = false;
  bool simulate_key_missing_ = false;
  bool simulate_sign_failure_ = false;
  bool simulate_corrupt_cert_ = false;
  std::vector<uint16_t> algorithm_preferences_ = {
      0x0401};  // SSL_SIGN_RSA_PKCS1_SHA256
  std::unique_ptr<CertificateSignalsCollector> collector_;
};

TEST_F(CertificateSignalsCollectorTest, GetSignal_MissingConsent) {
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates,
                        UserPermission::kMissingConsent, request, response,
                        run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(response.top_level_error.has_value());
  EXPECT_FALSE(response.certificate_signals_response.has_value());
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_MissingParameters) {
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifyFailureResponse(response, SignalCollectionError::kMissingParameters);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_MissingChallengeParameter) {
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  GetCertificateOptions options = CreateCertificateOptions(
      /*subject=*/base::DictValue(), /*issuer=*/base::DictValue());
  options.challenge = "";
  request.certificate_signal_parameters.push_back(std::move(options));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifyFailureResponse(response, SignalCollectionError::kMissingParameters);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_CorruptCertificate) {
  simulate_corrupt_cert_ = true;
  SetCertificatesToReturn(1, std::string(CN_KEY) + "=Test");
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(CreateCertificateOptions(
      /*subject=*/CreateFilter("Test"), /*issuer=*/base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/0u, /*cert_cn=*/"");
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_NullCertStore) {
  simulate_null_store_ = true;
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(CreateCertificateOptions(
      /*subject=*/base::DictValue(), /*issuer=*/base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/0u, /*cert_cn=*/"");
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_PrivateKeyMissing) {
  simulate_key_missing_ = true;
  SetCertificatesToReturn(1, std::string(CN_KEY) + "=Test");
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(CreateCertificateOptions(
      /*subject=*/CreateFilter("Test"), /*issuer=*/base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/0u, /*cert_cn=*/"");
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Certificates.Error",
      CertificateCollectionError::kPrivateKeyAcquisitionFailed, 1);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_SigningFails) {
  simulate_sign_failure_ = true;
  SetCertificatesToReturn(1, std::string(CN_KEY) + "=Test");
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(CreateCertificateOptions(
      /*subject=*/CreateFilter("Test"), /*issuer=*/base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/0u, /*cert_cn=*/"");
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Certificates.Error",
      CertificateCollectionError::kSigningFailed, 1);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_Success) {
  std::string cert_cn = "Test";
  SetCertificatesToReturn(1, std::string(CN_KEY) + "=" + cert_cn);
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(CreateCertificateOptions(
      /*subject=*/CreateFilter(cert_cn), /*issuer=*/base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/1u, cert_cn);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_FiltersOutMismatchedIssuer) {
  std::string cert_cn = "Test";
  SetCertificatesToReturn(1, std::string(CN_KEY) + "=" + cert_cn);
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(
      CreateCertificateOptions(/*subject=*/CreateFilter(cert_cn),
                               /*issuer=*/CreateFilter("UnknownIssuer")));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/0u, /*cert_cn=*/"");
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_FiltersOutMismatchedSubject) {
  std::string cert_cn = "Test";
  SetCertificatesToReturn(1, std::string(CN_KEY) + "=" + cert_cn);
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(
      CreateCertificateOptions(/*subject=*/CreateFilter("UnknownSubject"),
                               /*issuer=*/CreateFilter(cert_cn)));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/0u, /*cert_cn=*/"");
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_Success_TruncatesCerts) {
  std::string cert_cn = "TruncateTest";
  SetCertificatesToReturn(55, std::string(CN_KEY) + "=" + cert_cn);
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(CreateCertificateOptions(
      /*subject=*/CreateFilter(cert_cn), /*issuer=*/base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/50u, cert_cn,
                        /*expected_truncated=*/true);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest,
       GetSignal_Success_TruncatesCerts_IssuancePriority) {
  std::string newer_cn = "NewerCert";
  std::string oldest_cn = "OldestCert";
  cn_to_return_.clear();
  cert_start_times_.clear();
  base::Time now = base::Time::Now();
  for (int i = 0; i < 50; ++i) {
    cn_to_return_.push_back(std::string(CN_KEY) + "=" + newer_cn);
    cert_start_times_.push_back(now);
  }
  cn_to_return_.push_back(std::string(CN_KEY) + "=" + oldest_cn);
  cert_start_times_.push_back(now - base::Days(10));
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(
      CreateCertificateOptions(base::DictValue(), base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/50u, newer_cn,
                        /*expected_truncated=*/true);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest,
       GetSignal_Success_FiltersUnmatchedCerts) {
  std::string cert_cn = "UnmatchedCert";
  SetCertificatesToReturn(5, std::string(CN_KEY) + "=" + cert_cn);
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(CreateCertificateOptions(
      /*subject=*/CreateFilter("Impossible"), /*issuer=*/base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/0u, /*cert_cn=*/"");
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_Success_FiltersMixedCerts) {
  std::string match_cn = "MatchingCert";
  std::string fail_cn = "FailingCert";
  cn_to_return_.clear();
  cn_to_return_.push_back(std::string(CN_KEY) + "=" + match_cn);
  cn_to_return_.push_back(std::string(CN_KEY) + "=" + fail_cn);
  cn_to_return_.push_back(std::string(CN_KEY) + "=" + fail_cn);
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(CreateCertificateOptions(
      /*subject=*/CreateFilter(match_cn), /*issuer=*/base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/1u, match_cn);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_ConsumerUser) {
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates,
                        UserPermission::kConsumerUser, request, response,
                        run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(response.top_level_error.has_value());
  EXPECT_FALSE(response.certificate_signals_response.has_value());
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_UnknownUser) {
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kUnknownUser,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(response.top_level_error.has_value());
  EXPECT_FALSE(response.certificate_signals_response.has_value());
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_MultipleChallenges) {
  std::string cert_cn_a = "CertA";
  std::string cert_cn_b = "CertB";
  cn_to_return_.clear();
  cn_to_return_.push_back(std::string(CN_KEY) + "=" + cert_cn_a);
  cn_to_return_.push_back(std::string(CN_KEY) + "=" + cert_cn_b);

  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);

  GetCertificateOptions options_a;
  options_a.subject_pattern = GetCertPattern(CreateFilter(cert_cn_a));
  options_a.challenge = "ChallengeA";

  GetCertificateOptions options_b;
  options_b.subject_pattern = GetCertPattern(CreateFilter(cert_cn_b));
  options_b.challenge = "ChallengeB";

  request.certificate_signal_parameters.push_back(options_a);
  request.certificate_signal_parameters.push_back(options_b);

  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(response.certificate_signals_response.has_value());
  EXPECT_FALSE(
      response.certificate_signals_response->collection_error.has_value());
  EXPECT_EQ(
      response.certificate_signals_response->serialized_caa_responses.size(),
      2u);

  bool found_a = false;
  bool found_b = false;

  for (const auto& serialized_response :
       response.certificate_signals_response->serialized_caa_responses) {
    enterprise_management::SignedCertificateDetails signed_details;
    ASSERT_TRUE(signed_details.ParseFromString(serialized_response));
    EXPECT_FALSE(signed_details.signature().empty());

    enterprise_management::CertificateDetails inner_cert_details;
    ASSERT_TRUE(inner_cert_details.ParseFromString(signed_details.data()));
    EXPECT_FALSE(inner_cert_details.certificate().empty());

    const std::string& cert_bytes_str = inner_cert_details.certificate();
    scoped_refptr<net::X509Certificate> parsed_cert =
        net::X509Certificate::CreateFromBytes(
            base::as_byte_span(cert_bytes_str));
    ASSERT_TRUE(parsed_cert);

    if (parsed_cert->subject().common_name == cert_cn_a) {
      EXPECT_EQ(inner_cert_details.challenge(), "ChallengeA");
      found_a = true;
    } else if (parsed_cert->subject().common_name == cert_cn_b) {
      EXPECT_EQ(inner_cert_details.challenge(), "ChallengeB");
      found_b = true;
    }
  }
  EXPECT_TRUE(found_a);
  EXPECT_TRUE(found_b);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Certificates.Error", 0);
}

TEST_F(CertificateSignalsCollectorTest, GetSignal_NoSupportedAlgorithm) {
  std::string cert_cn = "Test";
  SetCertificatesToReturn(1, std::string(CN_KEY) + "=" + cert_cn);
  algorithm_preferences_ = {0x1234};

  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kCertificates);
  request.certificate_signal_parameters.push_back(CreateCertificateOptions(
      /*subject=*/CreateFilter(cert_cn), /*issuer=*/base::DictValue()));
  SignalsAggregationResponse response;
  InitializeCollector();

  base::RunLoop run_loop;
  collector_->GetSignal(SignalName::kCertificates, UserPermission::kGranted,
                        request, response, run_loop.QuitClosure());
  run_loop.Run();

  VerifySuccessResponse(response, /*expected_count=*/0u, /*cert_cn=*/"");
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Certificates.Error",
      CertificateCollectionError::kNoSupportedAlgorithm, 1);
}

}  // namespace device_signals
