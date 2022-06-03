// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/activation_list.h"

#include <ostream>

#include "base/notreached.h"

namespace subresource_filter {

std::ostream& operator<<(std::ostream& os, const ActivationList& type) {
  switch (type) {
    case ActivationList::NONE:
      os << "NONE";
      break;
    case ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL:
      os << "SOCIAL_ENG_ADS_INTERSTITIAL";
      break;
    case ActivationList::PHISHING_INTERSTITIAL:
      os << "PHISHING_INTERSTITIAL";
      break;
    case ActivationList::SUBRESOURCE_FILTER:
      os << "SUBRESOURCE_FILTER";
      break;
    case ActivationList::BETTER_ADS:
      os << "BETTER_ADS";
      break;
    case ActivationList::ABUSIVE:
      os << "ABUSIVE";
      break;
    default:
      NOTREACHED();
      break;
  }
  return os;
}

}  // namespace subresource_filter
