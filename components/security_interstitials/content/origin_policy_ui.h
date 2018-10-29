// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_ORIGIN_POLICY_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_ORIGIN_POLICY_UI_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"

class GURL;
namespace url {
class Origin;
}  // namespace url
namespace content {
enum class OriginPolicyErrorReason;
}  // namespace content

namespace security_interstitials {

// A helper class to build the error page for Origin Policy errors.
class OriginPolicyUI {
 public:
  static base::Optional<std::string> GetErrorPage(
      content::OriginPolicyErrorReason error_reason,
      const url::Origin& origin,
      const GURL& url);
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_ORIGIN_POLICY_UI_H_
