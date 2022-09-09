// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class BinaryFeatureExtractorWinTest : public testing::Test {
 protected:
  void SetUp() override {
    base::FilePath source_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &source_path));
    testdata_path_ = source_path
        .AppendASCII("safe_browsing")
        .AppendASCII("download_protection");

    binary_feature_extractor_ = new BinaryFeatureExtractor();
  }

  // Given a certificate chain protobuf, parse it into X509Certificates.
  void ParseCertificateChain(
      const ClientDownloadRequest_CertificateChain& chain,
      std::vector<scoped_refptr<net::X509Certificate> >* certs) {
    for (int i = 0; i < chain.element_size(); ++i) {
      scoped_refptr<net::X509Certificate> cert =
          net::X509Certificate::CreateFromBytes(
              base::as_bytes(base::make_span(chain.element(i).certificate())));
      if (cert)
        certs->push_back(cert);
    }
  }

  base::FilePath testdata_path_;
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;
};

TEST_F(BinaryFeatureExtractorWinTest, UntrustedSignedBinary) {
  // signed.exe is signed by an untrusted root CA.
  ClientDownloadRequest_SignatureInfo signature_info;
  binary_feature_extractor_->CheckSignature(
      testdata_path_.Append(L"signed.exe"),
      &signature_info);
  ASSERT_EQ(1, signature_info.certificate_chain_size());
  std::vector<scoped_refptr<net::X509Certificate> > certs;
  ParseCertificateChain(signature_info.certificate_chain(0), &certs);
  ASSERT_EQ(2u, certs.size());
  EXPECT_EQ("Joe's-Software-Emporium", certs[0]->subject().common_name);
  EXPECT_EQ("Root Agency", certs[1]->subject().common_name);

  EXPECT_TRUE(signature_info.has_trusted());
  EXPECT_FALSE(signature_info.trusted());
}

TEST_F(BinaryFeatureExtractorWinTest, TrustedBinary) {
  // disable_outdated_build_detector.exe is dual signed using Google's signing
  // certifiacte.
  ClientDownloadRequest_SignatureInfo signature_info;
  binary_feature_extractor_->CheckSignature(
      testdata_path_.Append(L"disable_outdated_build_detector.exe"),
      &signature_info);
  ASSERT_EQ(1, signature_info.certificate_chain_size());
  std::vector<scoped_refptr<net::X509Certificate> > certs;
  ParseCertificateChain(signature_info.certificate_chain(0), &certs);
  ASSERT_EQ(3u, certs.size());

  EXPECT_EQ("Google Inc", certs[0]->subject().common_name);
  EXPECT_EQ("VeriSign Class 3 Code Signing 2010 CA",
            certs[1]->subject().common_name);
  EXPECT_EQ("VeriSign Trust Network",
            certs[2]->subject().organization_unit_names[0]);

  EXPECT_TRUE(signature_info.trusted());
}

TEST_F(BinaryFeatureExtractorWinTest, UnsignedBinary) {
  // unsigned.exe has no signature information.
  ClientDownloadRequest_SignatureInfo signature_info;
  binary_feature_extractor_->CheckSignature(
      testdata_path_.Append(L"unsigned.exe"),
      &signature_info);
  EXPECT_EQ(0, signature_info.certificate_chain_size());
  EXPECT_FALSE(signature_info.has_trusted());
}

TEST_F(BinaryFeatureExtractorWinTest, NonExistentBinary) {
  // Test a file that doesn't exist.
  ClientDownloadRequest_SignatureInfo signature_info;
  binary_feature_extractor_->CheckSignature(
      testdata_path_.Append(L"doesnotexist.exe"),
      &signature_info);
  EXPECT_EQ(0, signature_info.certificate_chain_size());
  EXPECT_FALSE(signature_info.has_trusted());
}

TEST_F(BinaryFeatureExtractorWinTest, ExtractImageFeaturesNoFile) {
  // Test extracting headers from a file that doesn't exist.
  ClientDownloadRequest_ImageHeaders image_headers;
  ASSERT_FALSE(binary_feature_extractor_->ExtractImageFeatures(
      testdata_path_.AppendASCII("this_file_does_not_exist"),
      BinaryFeatureExtractor::kDefaultOptions, &image_headers,
      nullptr /* signed_data */));
  EXPECT_FALSE(image_headers.has_pe_headers());
}

TEST_F(BinaryFeatureExtractorWinTest, ExtractImageFeaturesNonImage) {
  // Test extracting headers from something that is not a PE image.
  ClientDownloadRequest_ImageHeaders image_headers;
  ASSERT_FALSE(binary_feature_extractor_->ExtractImageFeatures(
      testdata_path_.AppendASCII("simple_exe.cc"),
      BinaryFeatureExtractor::kDefaultOptions, &image_headers,
      nullptr /* signed_data */));
  EXPECT_FALSE(image_headers.has_pe_headers());
}

TEST_F(BinaryFeatureExtractorWinTest, ExtractImageFeatures) {
  // Test extracting features from something that is a PE image.
  ClientDownloadRequest_ImageHeaders image_headers;
  google::protobuf::RepeatedPtrField<std::string> signed_data;
  ASSERT_TRUE(binary_feature_extractor_->ExtractImageFeatures(
      testdata_path_.AppendASCII("unsigned.exe"),
      BinaryFeatureExtractor::kDefaultOptions, &image_headers, &signed_data));
  EXPECT_TRUE(image_headers.has_pe_headers());
  const ClientDownloadRequest_PEImageHeaders& pe_headers =
      image_headers.pe_headers();
  EXPECT_TRUE(pe_headers.has_dos_header());
  EXPECT_TRUE(pe_headers.has_file_header());
  EXPECT_TRUE(pe_headers.has_optional_headers32());
  EXPECT_FALSE(pe_headers.has_optional_headers64());
  EXPECT_NE(0, pe_headers.section_header_size());
  EXPECT_FALSE(pe_headers.has_export_section_data());
  EXPECT_EQ(0, pe_headers.debug_data_size());
  EXPECT_EQ(0, signed_data.size());
}

TEST_F(BinaryFeatureExtractorWinTest, ExtractImageFeaturesWithDebugData) {
  // Test extracting headers from something that is a PE image with debug data.
  ClientDownloadRequest_ImageHeaders image_headers;
  ASSERT_TRUE(binary_feature_extractor_->ExtractImageFeatures(
      testdata_path_.DirName().AppendASCII("module_with_exports_x86.dll"),
      BinaryFeatureExtractor::kDefaultOptions, &image_headers,
      nullptr /* signed_data */));
  EXPECT_TRUE(image_headers.has_pe_headers());
  const ClientDownloadRequest_PEImageHeaders& pe_headers =
      image_headers.pe_headers();
  EXPECT_TRUE(pe_headers.has_dos_header());
  EXPECT_TRUE(pe_headers.has_file_header());
  EXPECT_TRUE(pe_headers.has_optional_headers32());
  EXPECT_FALSE(pe_headers.has_optional_headers64());
  EXPECT_NE(0, pe_headers.section_header_size());
  EXPECT_TRUE(pe_headers.has_export_section_data());
  EXPECT_EQ(1, pe_headers.debug_data_size());
}

TEST_F(BinaryFeatureExtractorWinTest, ExtractImageFeaturesWithoutExports) {
  // Test extracting headers from something that is a PE image with debug data.
  ClientDownloadRequest_ImageHeaders image_headers;
  ASSERT_TRUE(binary_feature_extractor_->ExtractImageFeatures(
      testdata_path_.DirName().AppendASCII("module_with_exports_x86.dll"),
      BinaryFeatureExtractor::kOmitExports, &image_headers,
      nullptr /* signed_data */));
  EXPECT_TRUE(image_headers.has_pe_headers());
  const ClientDownloadRequest_PEImageHeaders& pe_headers =
      image_headers.pe_headers();
  EXPECT_TRUE(pe_headers.has_dos_header());
  EXPECT_TRUE(pe_headers.has_file_header());
  EXPECT_TRUE(pe_headers.has_optional_headers32());
  EXPECT_FALSE(pe_headers.has_optional_headers64());
  EXPECT_NE(0, pe_headers.section_header_size());
  EXPECT_FALSE(pe_headers.has_export_section_data());
  EXPECT_EQ(1, pe_headers.debug_data_size());
}

TEST_F(BinaryFeatureExtractorWinTest, ExtractImageFeaturesUntrustedSigned) {
  // Test extracting features from a signed PE image.
  ClientDownloadRequest_ImageHeaders image_headers;
  google::protobuf::RepeatedPtrField<std::string> signed_data;
  ASSERT_TRUE(binary_feature_extractor_->ExtractImageFeatures(
      testdata_path_.AppendASCII("signed.exe"),
      BinaryFeatureExtractor::kDefaultOptions, &image_headers, &signed_data));
  ASSERT_EQ(1, signed_data.size());
  ASSERT_LT(0U, signed_data.Get(0).size());
}

TEST_F(BinaryFeatureExtractorWinTest, ExtractImageFeaturesTrustedSigned) {
  // Test extracting features from a signed PE image from a trusted root.
  ClientDownloadRequest_ImageHeaders image_headers;
  google::protobuf::RepeatedPtrField<std::string> signed_data;
  ASSERT_TRUE(binary_feature_extractor_->ExtractImageFeatures(
      testdata_path_.AppendASCII("disable_outdated_build_detector.exe"),
      BinaryFeatureExtractor::kDefaultOptions, &image_headers, &signed_data));
  ASSERT_EQ(1, signed_data.size());
  ASSERT_LT(0U, signed_data.Get(0).size());
}

}  // namespace safe_browsing
