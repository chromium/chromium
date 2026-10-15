// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/tls_certificate_repository.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/attestation/oak_ct_verifier.h"
#include "components/private_ai/tls_certificate_downloader.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace private_ai {
namespace {

constexpr char kTestTcaPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDljCCAn6gAwIBAgIUfCLXRTF+hvOZMRMCihlu2cmsyAgwDQYJKoZIhvcNAQEL\n"
    "BQAwYzELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcM\n"
    "DU1vdW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3QgQ0ExFTATBgNVBAMMDFRlc3Qg\n"
    "Um9vdCBDQTAeFw0yNTEwMTAxNzEyMTlaFw0zNTEwMDgxNzEyMTlaMGMxCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRYwFAYDVQQHDA1Nb3VudGFpbiBW\n"
    "aWV3MRAwDgYDVQQKDAdUZXN0IENBMRUwEwYDVQQDDAxUZXN0IFJvb3QgQ0EwggEi\n"
    "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDGgR+Sc7ZYhdmNrLcg/ce/QLLq\n"
    "+uULUgGPmsHreoDB84mkPtUbYcy1z4CxGtu7JeAYv5JpJlDN5z//DTy0HxQSq2c3\n"
    "3gcDbBJ0gjasw9TTZJ+R7Vv2qXqknJjoZWyU4ctVc674HVCweOV0/7E3LMsZPaSM\n"
    "53ZOhlw/37PtRSNPVJszxoleEx3dfVmlBzQohicf+p5TTyq2Qq03EmL1cja2AhJA\n"
    "RP7HnpWJQ1FetG7HZ4BYQ77MByi9Wf8cTI2QQvTP/VQAT0hyK+FnPIQXaJW/ygd7\n"
    "34adVuMy43CHt/g69+NuZRR8u3a3F/FCjG8qNGQQNRSMhfZXv/NcVZ2tAxDzAgMB\n"
    "AAGjQjBAMA8GA1UdEwEB/wQFMAMBAf8wHQYDVR0OBBYEFJsmC4qYqbsduR8c4xpA\n"
    "M+2OF4irMA4GA1UdDwEB/wQEAwIBBjANBgkqhkiG9w0BAQsFAAOCAQEATr+SNcNa\n"
    "ZP+LCvF4582Pi/yZG1TAgUe14SLU0AA+BtBYDxDXaWll2n3Tz/qohoU+keG3vHlx\n"
    "fkNyggmDdefiw3aT7UGpLCmp1z7I43q0w3fw0B8lvMXl1fvl7MZB1MwtcKhLs6iJ\n"
    "DzPe6HKKTsRQb6g15Hc8eM+9hiilF2Am3ajzCjAVbc0zkuKcDUMWnic4xZ8eRByz\n"
    "I7pmUcPyqg/cvdqb4okU1vMdxyKXdu+DCPSoL+6e1JpvZWjUMsHmUb6Cf7/ibaIK\n"
    "+MpM9M9PGrqAsc4+uWRL/P6XMeg53k6KhnqYCDQlZ5Gh1NNu9YhmKwB1p6lo9w4h\n"
    "sixcP/zVzEuAHw==\n"
    "-----END CERTIFICATE-----\n";

constexpr char kTestOakCtPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDzzCCAregAwIBAgIRAPn88GGllak+BLKA2eRN3RcwDQYJKoZIhvcNAQELBQAw\n"
    "YzELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1v\n"
    "dW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3QgQ0ExFTATBgNVBAMMDFRlc3QgUm9v\n"
    "dCBDQTAeFw0yNTEwMTAxNzEyMTlaFw0yNzEwMTAxNzEyMTlaMGAxCzAJBgNVBAYT\n"
    "AlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRYwFAYDVQQHDA1Nb3VudGFpbiBWaWV3\n"
    "MRAwDgYDVQQKDAdUZXN0IENBMRIwEAYDVQQDDAkxMjcuMC4wLjEwggEiMA0GCSqG\n"
    "SIb3DQEBAQUAA4IBDwAwggEKAoIBAQDgU/TzmMEUMwLIpG3+qir3lD2mbwDfO95M\n"
    "n6PqB9Ss5VsN0azg7fnFmB01LeWzSZcUhUQP3EzSZwiIAaXYp+uT0Wqh91HnhH5S\n"
    "Kn28bw7Y27amPt7c9aRolkQRhQLtRxLfuGBxlXtih2h6RFYJ1bTI8fbJRpKLaOiD\n"
    "1dWGcSPDgB6/bAHH0qS8QG3g48AuMHi9rd0lZtP1BwdW187icsUlfQzhp28AqNqr\n"
    "S1RDCWSktlI4L7fMAd0cAycDR7/f5jew7RjcUQvUdSLfUHs86zc5HJtvCHunBayM\n"
    "Q/fx2lEGs4JFPsiBc56wpc92lq+BLKwBKkpYSx2+/x+Fwife8XgLAgMBAAGjgYAw\n"
    "fjAMBgNVHRMBAf8EAjAAMB0GA1UdDgQWBBTi4KRzlZvpbv3OKcRvB4ELlr1HujAf\n"
    "BgNVHSMEGDAWgBSbJguKmKm7HbkfHOMaQDPtjheIqzAdBgNVHSUEFjAUBggrBgEF\n"
    "BQcDAQYIKwYBBQUHAwIwDwYDVR0RBAgwBocEfwAAATANBgkqhkiG9w0BAQsFAAOC\n"
    "AQEAZnCA2uXBuGAZWC9nP+Aipl2GZKU8DvYBPlrLApNykuD5TwlqpcmM8rz8tJPu\n"
    "0NdXyIfReY4ixmRx3CPqtIf6UBiZuX8H6zPR+123A4f/+bCzH3n2BRlTsHQIRxN7\n"
    "NfoOx1sHHdxQb4vqKO48q5y5BctRhAScKh5qCBTbKldFgrAUx7VgigjP4fsIdGQ2\n"
    "0ADBBabyk6zvtpCfO/dhOyWk00iBtK2islyuRrNwOYATB4BMB9cLSXdTHBiFqNGT\n"
    "PlX7MPNj3JXsFYUXJ+SwI5GdDcQd/VrkFCJ7ic7tByEbEEkz3uGpEv1jYOypsqh0\n"
    "r+Wnaq2GThAxh+xXDozYtXYwpw==\n"
    "-----END CERTIFICATE-----\n";

constexpr char kNamespace[] = "tca";
constexpr char kBaseDomain[] = "oakct.transparentrelease.goog";
constexpr char kHistogram[] = "PrivateAi.Certs.OakCtVerificationResult";
constexpr char kDownloadSuccessHistogram[] = "PrivateAi.Certs.DownloadSuccess";

// A NetworkContext whose VerifyCert records its arguments and answers with a
// canned result, either immediately or when the test asks for it.
class FakeNetworkContext : public network::TestNetworkContext {
 public:
  void set_net_error(int net_error) { net_error_ = net_error; }
  void set_cert_verify_result(const net::CertVerifyResult& result) {
    result_ = result;
  }
  void set_defer_replies(bool defer) { defer_replies_ = defer; }

  int call_count() const { return call_count_; }
  bool has_pending_reply() const { return !pending_callbacks_.empty(); }
  const scoped_refptr<net::X509Certificate>& last_certificate() const {
    return last_certificate_;
  }
  const net::HostPortPair& last_host_port() const { return last_host_port_; }

  // Answers the oldest deferred VerifyCert call with the configured result.
  void RunPendingReply() {
    ASSERT_TRUE(has_pending_reply());
    VerifyCertCallback callback = std::move(pending_callbacks_.front());
    pending_callbacks_.pop_front();
    std::move(callback).Run(net_error_, result_, /*pkp_bypassed=*/false);
  }

  // network::TestNetworkContext:
  void VerifyCert(const scoped_refptr<net::X509Certificate>& certificate,
                  const net::HostPortPair& host_port,
                  const std::string& ocsp_result,
                  const std::string& sct_list,
                  VerifyCertCallback callback) override {
    ++call_count_;
    last_certificate_ = certificate;
    last_host_port_ = host_port;
    if (defer_replies_) {
      pending_callbacks_.push_back(std::move(callback));
      return;
    }
    std::move(callback).Run(net_error_, result_, /*pkp_bypassed=*/false);
  }

 private:
  int net_error_ = net::OK;
  net::CertVerifyResult result_;
  bool defer_replies_ = false;
  int call_count_ = 0;
  scoped_refptr<net::X509Certificate> last_certificate_;
  net::HostPortPair last_host_port_;
  base::circular_deque<VerifyCertCallback> pending_callbacks_;
};

net::CertVerifyResult CompliantResult() {
  net::CertVerifyResult result;
  result.is_issued_by_known_root = true;
  result.policy_compliance =
      net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
  return result;
}

using PemBundle = TlsCertificateDownloader::PemBundle;

// A downloader that answers with a canned PemBundle, either on the next task
// or when the test asks for it.
class FakeTlsCertificateDownloader : public TlsCertificateDownloader {
 public:
  FakeTlsCertificateDownloader()
      : TlsCertificateDownloader(GURL("https://example.com/tca.pem"),
                                 GURL("https://example.com/oakct.pem"),
                                 network::NetworkContextGetter()) {}
  ~FakeTlsCertificateDownloader() override = default;

  void set_result(std::optional<PemBundle> result) {
    result_ = std::move(result);
  }
  void set_defer_replies(bool defer) { defer_replies_ = defer; }

  int download_count() const { return download_count_; }
  bool has_pending_reply() const { return !pending_callbacks_.empty(); }

  void RunPendingReply() {
    ASSERT_TRUE(has_pending_reply());
    DownloadCallback callback = std::move(pending_callbacks_.front());
    pending_callbacks_.pop_front();
    std::move(callback).Run(result_);
  }

  // TlsCertificateDownloader:
  void Download(DownloadCallback callback) override {
    ++download_count_;
    if (defer_replies_) {
      pending_callbacks_.push_back(std::move(callback));
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result_));
  }

 private:
  std::optional<PemBundle> result_ =
      PemBundle{.tca_pem = kTestTcaPem, .oakct_pem = kTestOakCtPem};
  bool defer_replies_ = false;
  int download_count_ = 0;
  base::circular_deque<DownloadCallback> pending_callbacks_;
};

class TlsCertificateRepositoryTest : public testing::Test {
 public:
  void SetUp() override {
    // By default the verifier accepts the OakCT certificate.
    network_context_.set_cert_verify_result(CompliantResult());

    CreateRepository();
  }

  void TearDown() override {
    downloader_ = nullptr;
    repository_.reset();
  }

 protected:
  network::NetworkContextGetter GetNetworkContextGetter() {
    return base::BindRepeating(
        [](network::mojom::NetworkContext* context) { return context; },
        base::Unretained(&network_context_));
  }

  // (Re)creates `repository_` with a fresh fake downloader.
  void CreateRepository() {
    downloader_ = nullptr;
    auto downloader = std::make_unique<FakeTlsCertificateDownloader>();
    downloader_ = downloader.get();
    repository_ = std::make_unique<TlsCertificateRepository>(
        std::move(downloader), kNamespace, kBaseDomain,
        GetNetworkContextGetter());
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  FakeNetworkContext network_context_;
  std::unique_ptr<TlsCertificateRepository> repository_;
  raw_ptr<FakeTlsCertificateDownloader> downloader_ = nullptr;
};

TEST_F(TlsCertificateRepositoryTest, GetCertificatesDownloadsAndVerifies) {
  base::test::TestFuture<std::optional<TlsCertificates>> future;
  repository_->GetCertificates(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(downloader_->download_count(), 1);
  histogram_tester_.ExpectUniqueSample(kDownloadSuccessHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kHistogram,
                                       OakCtVerifier::Result::kVerified, 1);
}

TEST_F(TlsCertificateRepositoryTest, VerifiesOakCtChainForTcaRootHostname) {
  base::test::TestFuture<std::optional<TlsCertificates>> future;
  repository_->GetCertificates(future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());

  // The hostname's ContentID is the hash of the TCA root PEM as published.
  EXPECT_EQ(network_context_.call_count(), 1);
  EXPECT_EQ(network_context_.last_host_port().host(),
            *OakCtVerifier::ComputeContentId(
                base::as_byte_span(std::string_view(kTestTcaPem))) +
                ".tca.oakct.transparentrelease.goog");
  // The OakCT chain is handed to the verifier as parsed from the PEM file.
  net::CertificateList oakct_certs =
      net::X509Certificate::CreateCertificateListFromBytes(
          base::as_byte_span(std::string_view(kTestOakCtPem)),
          net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(oakct_certs.size(), 1u);
  ASSERT_TRUE(network_context_.last_certificate());
  EXPECT_TRUE(net::x509_util::CryptoBufferEqual(
      network_context_.last_certificate()->cert_buffer(),
      oakct_certs[0]->cert_buffer()));
}

TEST_F(TlsCertificateRepositoryTest, GetCertificatesReturnsCached) {
  base::test::TestFuture<std::optional<TlsCertificates>> future1;
  repository_->GetCertificates(future1.GetCallback());
  EXPECT_TRUE(future1.Get().has_value());
  EXPECT_EQ(downloader_->download_count(), 1);

  // Served from memory: no new download and no new verification.
  base::test::TestFuture<std::optional<TlsCertificates>> future2;
  repository_->GetCertificates(future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
  auto result2 = future2.Get();
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result2->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(downloader_->download_count(), 1);
  EXPECT_EQ(network_context_.call_count(), 1);
}

TEST_F(TlsCertificateRepositoryTest, DownloadFailureReturnsNullopt) {
  downloader_->set_result(std::nullopt);

  base::test::TestFuture<std::optional<TlsCertificates>> future;
  repository_->GetCertificates(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(network_context_.call_count(), 0);
  histogram_tester_.ExpectUniqueSample(kDownloadSuccessHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(kHistogram, 0);
}

TEST_F(TlsCertificateRepositoryTest, FailureIsNotCached) {
  downloader_->set_result(std::nullopt);

  base::test::TestFuture<std::optional<TlsCertificates>> future1;
  repository_->GetCertificates(future1.GetCallback());
  EXPECT_FALSE(future1.Get().has_value());
  EXPECT_EQ(downloader_->download_count(), 1);

  // The next call downloads again.
  downloader_->set_result(
      PemBundle{.tca_pem = kTestTcaPem, .oakct_pem = kTestOakCtPem});
  base::test::TestFuture<std::optional<TlsCertificates>> future2;
  repository_->GetCertificates(future2.GetCallback());
  auto result2 = future2.Get();
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result2->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(downloader_->download_count(), 2);
}

TEST_F(TlsCertificateRepositoryTest, OakCtVerificationFailureReturnsNullopt) {
  network_context_.set_net_error(net::ERR_CERT_COMMON_NAME_INVALID);

  base::test::TestFuture<std::optional<TlsCertificates>> future;
  repository_->GetCertificates(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  histogram_tester_.ExpectUniqueSample(
      kHistogram, OakCtVerifier::Result::kCertVerifyFailed, 1);
}

TEST_F(TlsCertificateRepositoryTest, MalformedOakCtPemReturnsNullopt) {
  downloader_->set_result(PemBundle{
      .tca_pem = kTestTcaPem, .oakct_pem = "not a valid pem certificate"});

  base::test::TestFuture<std::optional<TlsCertificates>> future;
  repository_->GetCertificates(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(network_context_.call_count(), 0);
  histogram_tester_.ExpectUniqueSample(
      kHistogram, OakCtVerifier::Result::kInvalidOakCtCertificate, 1);
}

TEST_F(TlsCertificateRepositoryTest, NoNetworkContextReturnsNullopt) {
  TlsCertificateRepository repository(
      std::make_unique<FakeTlsCertificateDownloader>(), kNamespace, kBaseDomain,
      base::BindRepeating(
          []() -> network::mojom::NetworkContext* { return nullptr; }));

  base::test::TestFuture<std::optional<TlsCertificates>> future;
  repository.GetCertificates(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  histogram_tester_.ExpectUniqueSample(
      kHistogram, OakCtVerifier::Result::kNoNetworkContext, 1);
}

TEST_F(TlsCertificateRepositoryTest, ConcurrentGetCertificatesCoalesced) {
  downloader_->set_defer_replies(true);

  base::test::TestFuture<std::optional<TlsCertificates>> future1;
  base::test::TestFuture<std::optional<TlsCertificates>> future2;
  base::test::TestFuture<std::optional<TlsCertificates>> future3;

  repository_->GetCertificates(future1.GetCallback());
  repository_->GetCertificates(future2.GetCallback());
  repository_->GetCertificates(future3.GetCallback());

  // Only one download initiated.
  EXPECT_EQ(downloader_->download_count(), 1);

  downloader_->RunPendingReply();

  EXPECT_EQ(future1.Get()->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(future2.Get()->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(future3.Get()->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(network_context_.call_count(), 1);
}

TEST_F(TlsCertificateRepositoryTest, GetCertificatesWhileVerifyingCoalesced) {
  network_context_.set_defer_replies(true);

  base::test::TestFuture<std::optional<TlsCertificates>> future1;
  repository_->GetCertificates(future1.GetCallback());

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return network_context_.has_pending_reply(); }));

  base::test::TestFuture<std::optional<TlsCertificates>> future2;
  repository_->GetCertificates(future2.GetCallback());
  EXPECT_EQ(downloader_->download_count(), 1);

  network_context_.RunPendingReply();

  EXPECT_EQ(future1.Get()->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(future2.Get()->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(network_context_.call_count(), 1);
}

TEST_F(TlsCertificateRepositoryTest, ForceRefreshClearsCacheAndDownloads) {
  base::test::TestFuture<std::optional<TlsCertificates>> future1;
  repository_->GetCertificates(future1.GetCallback());
  ASSERT_TRUE(future1.Get().has_value());
  EXPECT_EQ(future1.Get()->tca_root_pem, kTestTcaPem);

  // Serve a different "TCA root" from now on (the verifier accepts anything).
  downloader_->set_result(
      PemBundle{.tca_pem = kTestOakCtPem, .oakct_pem = kTestOakCtPem});

  base::test::TestFuture<std::optional<TlsCertificates>> future2;
  repository_->ForceRefresh(future2.GetCallback());
  auto result2 = future2.Get();
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result2->tca_root_pem, kTestOakCtPem);
  EXPECT_EQ(downloader_->download_count(), 2);
  EXPECT_EQ(network_context_.call_count(), 2);

  // The refreshed certificates are now the ones served from memory.
  base::test::TestFuture<std::optional<TlsCertificates>> future3;
  repository_->GetCertificates(future3.GetCallback());
  EXPECT_EQ(future3.Get()->tca_root_pem, kTestOakCtPem);
  EXPECT_EQ(downloader_->download_count(), 2);
}

TEST_F(TlsCertificateRepositoryTest, ForceRefreshCoalescesWithInFlightLookup) {
  downloader_->set_defer_replies(true);

  base::test::TestFuture<std::optional<TlsCertificates>> initial_future;
  repository_->GetCertificates(initial_future.GetCallback());
  EXPECT_EQ(downloader_->download_count(), 1);

  // ForceRefresh joins the in-flight lookup without launching another.
  base::test::TestFuture<std::optional<TlsCertificates>> refresh_future;
  repository_->ForceRefresh(refresh_future.GetCallback());
  EXPECT_EQ(downloader_->download_count(), 1);

  downloader_->RunPendingReply();

  EXPECT_EQ(initial_future.Get()->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(refresh_future.Get()->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(downloader_->download_count(), 1);
}

TEST_F(TlsCertificateRepositoryTest, ConcurrentForceRefreshCoalesced) {
  downloader_->set_defer_replies(true);

  base::test::TestFuture<std::optional<TlsCertificates>> future1;
  base::test::TestFuture<std::optional<TlsCertificates>> future2;
  base::test::TestFuture<std::optional<TlsCertificates>> future3;

  repository_->ForceRefresh(future1.GetCallback());
  repository_->ForceRefresh(future2.GetCallback());
  repository_->ForceRefresh(future3.GetCallback());

  EXPECT_EQ(downloader_->download_count(), 1);

  downloader_->RunPendingReply();

  EXPECT_EQ(future1.Get()->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(future2.Get()->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(future3.Get()->tca_root_pem, kTestTcaPem);
}

TEST_F(TlsCertificateRepositoryTest, ReentrantForceRefreshInCallback) {
  base::test::TestFuture<std::optional<TlsCertificates>> future;
  repository_->GetCertificates(base::BindOnce(
      [](TlsCertificateRepository* repository,
         base::test::TestFuture<std::optional<TlsCertificates>>* future,
         std::optional<TlsCertificates> certs) {
        // Re-entrant call on the same sequence.
        repository->ForceRefresh(future->GetCallback());
      },
      repository_.get(), &future));

  // The re-entrant ForceRefresh starts a second lookup.
  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->tca_root_pem, kTestTcaPem);
  EXPECT_EQ(downloader_->download_count(), 2);
}

}  // namespace
}  // namespace private_ai
