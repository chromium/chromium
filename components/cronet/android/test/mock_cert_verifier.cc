// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/test/test_support_android.h"
#include "components/cronet/android/cronet_tests_jni_headers/MockCertVerifier_jni.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"

using base::android::JavaParamRef;

namespace cronet {

namespace {

// Populates |out_hash_value| with the SHA256 hash of the |cert| public key.
// Returns true on success.
static bool CalculatePublicKeySha256(const net::X509Certificate& cert,
                                     net::HashValue* out_hash_value) {
  // Extract the public key from the cert.
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(cert.cert_buffer()),
          &spki_bytes)) {
    LOG(INFO) << "Unable to retrieve the public key from the DER cert";
    return false;
  }
  // Calculate SHA256 hash of public key bytes.
  *out_hash_value = net::HashValue(net::HASH_VALUE_SHA256);
  crypto::SHA256HashString(spki_bytes, out_hash_value->data(),
                           crypto::kSHA256Length);
  return true;
}

}  // namespace

static jlong JNI_MockCertVerifier_CreateMockCertVerifier(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& jcerts,
    const jboolean jknown_root,
    const JavaParamRef<jstring>& jtest_data_dir) {
  base::FilePath test_data_dir(
      base::android::ConvertJavaStringToUTF8(env, jtest_data_dir));
  base::InitAndroidTestPaths(test_data_dir);

  std::vector<std::string> certs;
  base::android::AppendJavaStringArrayToStringVector(env, jcerts, &certs);
  net::MockCertVerifier* mock_cert_verifier = new net::MockCertVerifier();
  for (const auto& cert : certs) {
    net::CertVerifyResult verify_result;
    verify_result.verified_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), cert);

    // By default, HPKP verification is enabled for known trust roots only.
    verify_result.is_issued_by_known_root = jknown_root;

    // Calculate the public key hash and add it to the verify_result.
    net::HashValue hashValue;
    CHECK(CalculatePublicKeySha256(*verify_result.verified_cert.get(),
                                   &hashValue));
    verify_result.public_key_hashes.push_back(hashValue);

    mock_cert_verifier->AddResultForCert(verify_result.verified_cert.get(),
                                         verify_result, net::OK);
  }

  return reinterpret_cast<jlong>(mock_cert_verifier);
}

}  // namespace cronet
