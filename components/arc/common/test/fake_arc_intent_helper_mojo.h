// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_TEST_FAKE_ARC_INTENT_HELPER_MOJO_H_
#define COMPONENTS_ARC_COMMON_TEST_FAKE_ARC_INTENT_HELPER_MOJO_H_

#include <string>

#include "components/arc/common/intent_helper/arc_intent_helper_mojo_delegate.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace arc {

// For tests to wrap the real mojo connection.
class FakeArcIntentHelperMojo : public ArcIntentHelperMojoDelegate {
 public:
  FakeArcIntentHelperMojo();
  FakeArcIntentHelperMojo(const FakeArcIntentHelperMojo&) = delete;
  FakeArcIntentHelperMojo operator=(const FakeArcIntentHelperMojo&) = delete;
  ~FakeArcIntentHelperMojo() override;

  // ArcIntentHelperMojoDelegate:
  bool IsArcAvailable() override;
  bool IsRequestUrlHandlerListAvailable() override;
  bool IsRequestTextSelectionActionsAvailable() override;
  bool RequestUrlHandlerList(const std::string& url,
                             RequestUrlHandlerListCallback callback) override;
  bool RequestTextSelectionActions(
      const std::string& text,
      ui::ResourceScaleFactor scale_factor,
      RequestTextSelectionActionsCallback callback) override;
  bool HandleUrl(const std::string& url,
                 const std::string& package_name) override;
  bool HandleIntent(const IntentInfo& intent,
                    const ActivityName& activity) override;
  bool AddPreferredPackage(const std::string& package_name) override;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_TEST_FAKE_ARC_INTENT_HELPER_MOJO_H_
