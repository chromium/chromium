// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace kids_management {

class KidsChromeManagementClientForTesting : public KidsChromeManagementClient {
 public:
  KidsChromeManagementClientForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  ~KidsChromeManagementClientForTesting() override;

  void ClassifyURL(
      std::unique_ptr<kids_chrome_management::ClassifyUrlRequest> request_proto,
      KidsChromeManagementClient::KidsChromeManagementCallback callback)
      override;

  void SetResponseWithError(
      std::unique_ptr<kids_chrome_management::ClassifyUrlResponse>
          response_proto,
      KidsChromeManagementClient::ErrorCode error_code);

 private:
  std::unique_ptr<kids_chrome_management::ClassifyUrlResponse> response_proto_;
  KidsChromeManagementClient::ErrorCode error_code_;
};

kids_chrome_management::ClassifyUrlResponse::DisplayClassification
ConvertClassification(safe_search_api::ClientClassification classification);

// Returns a fake response proto with a response according to |classification|.
std::unique_ptr<kids_chrome_management::ClassifyUrlResponse> BuildResponseProto(
    safe_search_api::ClientClassification classification);

}  // namespace kids_management

namespace supervised_user {

void SetFamilyMemberAttributesForTesting(
    kids_chrome_management::FamilyMember* mutable_member,
    kids_chrome_management::FamilyRole role,
    base::StringPiece username);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_
