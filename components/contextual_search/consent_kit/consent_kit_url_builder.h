// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONSENT_KIT_CONSENT_KIT_URL_BUILDER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONSENT_KIT_CONSENT_KIT_URL_BUILDER_H_

#include <string>
#include <vector>

#include "url/gurl.h"

namespace drive {

// Utility to construct URLs for the ConsentKit embedded UI.
class ConsentKitUrlBuilder {
 public:
  ConsentKitUrlBuilder();
  ~ConsentKitUrlBuilder();

  // Not copyable or movable
  ConsentKitUrlBuilder(const ConsentKitUrlBuilder&) = delete;
  ConsentKitUrlBuilder& operator=(const ConsentKitUrlBuilder&) = delete;

  // Sets the user's session index (e.g., 0, 1).
  void SetSessionIndex(int session_index);

  // Sets the locale string (e.g., "en-US").
  void SetLocale(const std::string& locale);

  void SetFlowId(int32_t flow_id);
  void SetProductId(int32_t product_id);
  void SetEntrypointId(const std::string& entrypoint_id);
  // Sets the allowed host origins for the handshake.
  void SetHostOrigins(std::vector<std::string> host_origins);

  // Builds the ConsentKit URL. Returns an invalid GURL on serialization
  // failure.
  GURL Build();

 private:
  int session_index_ = 0;
  std::string locale_ = "en-US";
  int32_t flow_id_ = 0;
  int32_t product_id_ = 0;
  std::string entrypoint_id_;
  std::vector<std::string> host_origins_ = {
      "chrome-untrusted://drive-picker-host", "chrome://drive-picker-host"};
};

}  // namespace drive

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONSENT_KIT_CONSENT_KIT_URL_BUILDER_H_
