// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"

#include <utility>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using kids_chrome_management::ClassifyUrlResponse;

namespace kids_management {

KidsChromeManagementClientForTesting::KidsChromeManagementClientForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : KidsChromeManagementClient(url_loader_factory, identity_manager) {}

KidsChromeManagementClientForTesting::~KidsChromeManagementClientForTesting() =
    default;

void KidsChromeManagementClientForTesting::ClassifyURL(
    std::unique_ptr<kids_chrome_management::ClassifyUrlRequest> request_proto,
    KidsChromeManagementClient::KidsChromeManagementCallback callback) {
  // Drop the request if the response is not configured.
  if (response_proto_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::move(response_proto_), error_code_));
  }
}

void KidsChromeManagementClientForTesting::SetResponseWithError(
    std::unique_ptr<ClassifyUrlResponse> response_proto,
    KidsChromeManagementClient::ErrorCode error_code) {
  response_proto_ = std::move(response_proto);
  error_code_ = error_code;
}

ClassifyUrlResponse::DisplayClassification ConvertClassification(
    safe_search_api::ClientClassification classification) {
  switch (classification) {
    case safe_search_api::ClientClassification::kAllowed:
      return ClassifyUrlResponse::ALLOWED;
    case safe_search_api::ClientClassification::kRestricted:
      return ClassifyUrlResponse::RESTRICTED;
    case safe_search_api::ClientClassification::kUnknown:
      return ClassifyUrlResponse::UNKNOWN_DISPLAY_CLASSIFICATION;
  }
}

std::unique_ptr<ClassifyUrlResponse> BuildResponseProto(
    safe_search_api::ClientClassification classification) {
  auto response_proto = std::make_unique<ClassifyUrlResponse>();
  response_proto->set_display_classification(
      ConvertClassification(classification));
  return response_proto;
}

}  // namespace kids_management

namespace supervised_user {

void SetFamilyMemberAttributesForTesting(
    kids_chrome_management::FamilyMember* mutable_member,
    kids_chrome_management::FamilyRole role,
    base::StringPiece username) {
  mutable_member->mutable_profile()->set_display_name(std::string(username));
  mutable_member->mutable_profile()->set_email(
      base::StrCat({username, "@gmail.com"}));
  mutable_member->mutable_profile()->set_profile_url(
      base::StrCat({"http://profile.url/", username}));
  mutable_member->mutable_profile()->set_profile_image_url(
      base::StrCat({"http://image.url/", username}));
  mutable_member->set_role(role);
  mutable_member->set_user_id(base::StrCat({"obfuscatedGaiaId", username}));
}

}  // namespace supervised_user
