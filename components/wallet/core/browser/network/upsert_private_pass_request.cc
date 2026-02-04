// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/upsert_private_pass_request.h"

#include "base/json/json_writer.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace wallet {

namespace {

base::DictValue BuildClientInfo() {
  base::DictValue chrome_client_info =
      base::DictValue().Set("version", version_info::GetVersionNumber());

  return base::DictValue().Set("chrome_client_info",
                               std::move(chrome_client_info));
}

base::DictValue BuildPassportRequest(const Passport& passport) {
  // TODO(crbug.com/478783796): Implement Passport request building.
  return base::DictValue();
}

base::DictValue BuildDriverLicenseRequest(const DriverLicense& license) {
  // TODO(crbug.com/478783796): Implement DriverLicense request building.
  return base::DictValue();
}

base::DictValue BuildNationalIdentityCardRequest(
    const NationalIdentityCard& card) {
  // TODO(crbug.com/478783796): Implement NationalIdentityCard request building.
  return base::DictValue();
}

base::DictValue BuildKTNRequest(const KTN& ktn) {
  // TODO(crbug.com/478783796): Implement KTN request building.
  return base::DictValue();
}

base::DictValue BuildRedressNumberRequest(const RedressNumber& number) {
  // TODO(crbug.com/478783796): Implement RedressNumber request building.
  return base::DictValue();
}

base::DictValue BuildPrivatePassDict(const WalletPass& pass) {
  base::DictValue private_pass;
  if (pass.id) {
    private_pass.Set("pass_id", *pass.id);
  }

  std::visit(
      absl::Overload{
          [&private_pass](const Passport& passport) {
            private_pass.Set("passport", BuildPassportRequest(passport));
          },
          [&private_pass](const DriverLicense& license) {
            private_pass.Set("driver_license",
                             BuildDriverLicenseRequest(license));
          },
          [&private_pass](const NationalIdentityCard& card) {
            private_pass.Set("id_card", BuildNationalIdentityCardRequest(card));
          },
          [&private_pass](const KTN& ktn) {
            private_pass.Set("known_traveller_number", BuildKTNRequest(ktn));
          },
          [&private_pass](const RedressNumber& number) {
            private_pass.Set("redress_number",
                             BuildRedressNumberRequest(number));
          },
          [](const auto&) {
            // Other pass types are not supported as private passes.
            NOTREACHED();
          }},
      pass.pass_data);

  return private_pass;
}

}  // namespace

UpsertPrivatePassRequest::UpsertPrivatePassRequest(
    WalletPass pass,
    WalletHttpClient::UpsertPassCallback callback)
    : pass_(pass), callback_(std::move(callback)) {
  CHECK(callback_);
}

UpsertPrivatePassRequest::~UpsertPrivatePassRequest() = default;

std::string UpsertPrivatePassRequest::GetRequestUrlPath() const {
  return "v1/e/privatePasses:upsert";
}

std::string UpsertPrivatePassRequest::GetRequestContent() const {
  const base::DictValue request_dict =
      base::DictValue()
          .Set("private_pass", BuildPrivatePassDict(pass_))
          .Set("client_info", BuildClientInfo());
  return base::WriteJson(request_dict).value_or("");
}

void UpsertPrivatePassRequest::OnResponse(
    WalletHttpClient::HttpResponse http_response) && {
  if (!http_response.has_value()) {
    std::move(callback_).Run(base::unexpected(http_response.error()));
    return;
  }

  // TODO(crbug.com/468916773): Parse the response body to extract the pass_id.
  std::move(callback_).Run(WalletPass{});
}

}  // namespace wallet
