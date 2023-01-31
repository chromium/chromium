// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_POLICY_HEADLESS_MODE_POLICY_HANDLER_H_
#define COMPONENTS_HEADLESS_POLICY_HEADLESS_MODE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace headless {

// Handles the HeadlessMode policy. Controls the managed values of the
// |kHeadlessMode| pref.
class HeadlessModePolicyHandler : public policy::IntRangePolicyHandler {
 public:
  HeadlessModePolicyHandler();
  ~HeadlessModePolicyHandler() override;

  HeadlessModePolicyHandler(const HeadlessModePolicyHandler&) = delete;
  HeadlessModePolicyHandler& operator=(const HeadlessModePolicyHandler&) =
      delete;
};

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_POLICY_HEADLESS_MODE_POLICY_HANDLER_H_
