// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_DM_AUTH_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_DM_AUTH_H_

#include <memory>
#include <string>

#include "base/check_op.h"
#include "components/policy/policy_export.h"

namespace policy {

// The enum type for the token used for authentication. Explicit values are
// set to allow easy identification of value from the logs.
enum class DMAuthTokenType {
  kNoAuth = 0,
  // Skipping obsolete kGaia = 1
  kDm = 2,
  kEnrollment = 3,
  kOauth = 4,
  kOidc = 5,
};

// Class that encapsulates different authentication methods to interact with
// device management service.
// We currently have 3 methods for authentication:
// * OAuth token, that is passed as a part of URL
// * Enrollment token, provided by installation configuration, passed as
//       Authorization: GoogleEnrollmentToken header
// * DMToken, created during Register request, passed as
//     Authorization: GoogleDMToken header
// Also, several requests require no authentication (e.g. enterprise_check) or
// embed some authentication in the payload (e.g. certificate_based_register).
class POLICY_EXPORT DMAuth {
 public:
  // Static methods for creating DMAuth instances:
  static DMAuth FromDMToken(const std::string& dm_token);
  static DMAuth FromOAuthToken(const std::string& oauth_token);
  static DMAuth FromEnrollmentToken(const std::string& token);
  static DMAuth FromOidcResponse(const std::string& oidc_id_token);
  static DMAuth NoAuth();

  DMAuth();
  ~DMAuth();

  DMAuth(const DMAuth& other) = delete;
  DMAuth& operator=(const DMAuth& other) = delete;

  DMAuth(DMAuth&& other);
  DMAuth& operator=(DMAuth&& other);

  bool operator==(const DMAuth& other) const;
  bool operator!=(const DMAuth& other) const;

  // Creates a copy of DMAuth.
  DMAuth Clone() const;

  // Checks if no authentication is provided.
  bool empty() const { return token_type_ == DMAuthTokenType::kNoAuth; }

  std::string dm_token() const {
    DCHECK_EQ(DMAuthTokenType::kDm, token_type_);
    return token_;
  }
  bool has_dm_token() const { return token_type_ == DMAuthTokenType::kDm; }
  std::string enrollment_token() const {
    DCHECK_EQ(DMAuthTokenType::kEnrollment, token_type_);
    return token_;
  }
  bool has_enrollment_token() const {
    return token_type_ == DMAuthTokenType::kEnrollment;
  }
  std::string oauth_token() const {
    DCHECK(token_type_ == DMAuthTokenType::kOauth);
    return token_;
  }
  bool has_oauth_token() const {
    return token_type_ == DMAuthTokenType::kOauth;
  }
  std::string oidc_id_token() const {
    DCHECK_EQ(DMAuthTokenType::kOidc, token_type_);
    return token_;
  }
  bool has_oidc_id_token() const {
    return token_type_ == DMAuthTokenType::kOidc;
  }
  DMAuthTokenType token_type() const { return token_type_; }

 private:
  DMAuth(const std::string& token, DMAuthTokenType token_type);

  std::string token_;
  DMAuthTokenType token_type_ = DMAuthTokenType::kNoAuth;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_DM_AUTH_H_
