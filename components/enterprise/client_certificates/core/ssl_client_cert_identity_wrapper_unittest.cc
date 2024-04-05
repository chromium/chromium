// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ssl_client_cert_identity_wrapper.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/test_ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

namespace {

scoped_refptr<net::X509Certificate> LoadTestCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

}  // namespace

// Tests the creation of a SSLClientCertIdentityWrapper, and how calling
// AcquirePrivateKey will return the private key given at construction time.
TEST(SSLClientCertIdentityWrapperTest, AcquirePrivateKey) {
  auto fake_cert = LoadTestCert();

  // Using this fake key is fine as it won't be used to sign anything.
  auto mocked_private_key = net::CreateFailSigningSSLPrivateKey();

  SSLClientCertIdentityWrapper wrapper(fake_cert, mocked_private_key);

  EXPECT_EQ(wrapper.certificate(), fake_cert.get());

  base::test::TestFuture<scoped_refptr<net::SSLPrivateKey>> test_future;
  wrapper.AcquirePrivateKey(test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), mocked_private_key);
}

}  // namespace client_certificates
