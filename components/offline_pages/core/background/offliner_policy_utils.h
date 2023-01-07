// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_POLICY_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_POLICY_UTILS_H_

namespace offline_pages {

class OfflinerPolicy;
class SavePageRequest;

class OfflinerPolicyUtils {
 public:
  enum class RequestExpirationStatus {
    VALID,
    EXPIRED,
    START_COUNT_EXCEEDED,
    COMPLETION_COUNT_EXCEEDED,
  };

  static RequestExpirationStatus CheckRequestExpirationStatus(
      const SavePageRequest* request,
      const OfflinerPolicy* policy);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_POLICY_UTILS_H_
