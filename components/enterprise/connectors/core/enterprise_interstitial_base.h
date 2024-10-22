// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ENTERPRISE_INTERSTITIAL_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ENTERPRISE_INTERSTITIAL_BASE_H_

#include "base/values.h"
#include "components/security_interstitials/core/unsafe_resource.h"

namespace enterprise_connectors {

// Base class for cross-platform enterprise interstitial code used by the
// "EnterpriseRealTimeUrlCheckMode" policy. This is intended to be the base
// class of `SecurityInterstitialPage` and `IOSSecurityInterstitialPage`
// implementations.
class EnterpriseInterstitialBase {
 protected:
  // The type of interstitial represented by this class. This value will affect
  // what buttons are available on the interstitial page, what strings are
  // shown, etc.
  enum class Type {
    kWarn,
    kBlock,
  };
  virtual Type type() const = 0;

  // The `security_interstitials::UnsafeResource`s that triggered the
  // interstitial represented by this class.
  virtual const std::vector<security_interstitials::UnsafeResource>&
  unsafe_resources() const = 0;

  // Helper function to access `SecurityInterstitialPage::request_url()` or
  // `IOSSecurityInterstitialPage::request_url()` indirectly.
  virtual GURL request_url() const = 0;

  // Helper function to be used by
  // `SecurityInterstitialPage::PopulateInterstitialStrings` and
  // `IOSSecurityInterstitialPage::PopulateInterstitialStrings` overrides to
  // populate their strings.
  void PopulateStrings(base::Value::Dict& load_time_data) const;

 private:
  int GetPrimaryParagraphMessageId() const;
  int GetCustomMessagePrimaryParagraphMessageId() const;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ENTERPRISE_INTERSTITIAL_BASE_H_
