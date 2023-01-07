// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_SCOPE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_SCOPE_H_

#include <iosfwd>

namespace subresource_filter {

// Defines the scope of triggering the Safe Browsing Subresource Filter.
enum class ActivationScope {
  NO_SITES,
  // Allows to activate Safe Browsing Subresource Filter only on web sites from
  // the Safe Browsing blocklist.
  ACTIVATION_LIST,
  // Testing only. Allows to send activation signal to the RenderFrame for each
  // load.
  ALL_SITES,
  LAST = ALL_SITES,
};

// For logging use only.
std::ostream& operator<<(std::ostream& os, const ActivationScope& state);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_SCOPE_H_
