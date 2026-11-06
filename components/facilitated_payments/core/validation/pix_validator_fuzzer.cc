// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_view_rust.h"
#include "components/facilitated_payments/core/validation/payment_qr_validator_cxx.rs.h"
#include "components/facilitated_payments/core/validation/pix_validator_fuzzer_util.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace payments::facilitated {

void GetPixQrCodeTypeCanParseAnyString(const std::string& input) {
  get_pix_qr_code_type(base::StringViewToRustSlice(input));
}

FUZZ_TEST(GetPixQrCodeTypeTest, GetPixQrCodeTypeCanParseAnyString)
    .WithDomains(fuzztest::InRegexp(kPixValidatorFuzzerDomainRegexPattern))
    .WithSeeds({{"000201260063041D3D"},
                {"00020126030014br.gov.bcb.pix63041D3D"},
                {"00020126030014BR.GOV.BCB.PIX63041D3D"},
                {"00020126180014br.gov.bcb.pix620063041D3D"},
                {"00020126180014br.gov.bcb.pix630"},
                {"00020126180014br.gov.bcb.pix63041D3"},
                {"00020126180014br.gov.bcb.pix63041D3D"},
                {"00020126180014br.gov.bcb.PIX63041D3D"},
                {"00020126180014br.gov.bcb.pix63051D3D"},
                {"00020126180014br.gov.bcb.pix64041D3D"},
                {"000201261801020063041D3D"},
                {"00020163041D3D"},
                {"01020126180014br.gov.bcb.pix63041D3D"}});

void ValidatePaymentQrCodeCanParseAnyString(const std::string& input) {
  validate_payment_qr_code(base::StringViewToRustSlice(input));
}

// The seeds end with valid CRCs, so that mutations reach the scheme matching.
FUZZ_TEST(ValidatePaymentQrCodeTest, ValidatePaymentQrCodeCanParseAnyString)
    .WithDomains(fuzztest::InRegexp(kPixValidatorFuzzerDomainRegexPattern))
    .WithSeeds(
        {{"00020126370014br.gov.bcb.pix2515www.example.com63042003"},
         {"00020126580014br.gov.bcb.pix"
          "0136123e4567-e12b-12d1-a456-4266554400006304BB8F"},
         {"00020126280014A000000615000101068900005802MY6304DCBC"},
         {"000201010212"
          "29370016A00000067701011101130066812345678"
          "53037645406150.005802TH6304C40C"},
         {"00020151440014ID.CO.QRIS.WWW0215ID10200123456780303UMI"
          "53033605802ID630453A1"},
         {"00020126420016ID.CO.TELKOM.WWW0118936000091234567890"
          "53033605802ID6304353C"},
         {"00020101021226430017MY.COM.AIRPAY.WWW0118000000000000000000"
          "53034585802MY63049A9D"},
         {"00020101021226310007vn.momo0116MOMOEXAMPLE00001"
          "38280010A0000007270110000697042253037045802VN6304D2CA"},
         {"00020126280015com.example.pay0105123456304FD43"},
         {"00020126370014br.gov.bcb.pix2515www.example.com62006304BBBE"}});

}  // namespace payments::facilitated
