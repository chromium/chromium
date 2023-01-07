// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/test/fake_arc_intent_helper_mojo.h"

namespace arc {

FakeArcIntentHelperMojo::FakeArcIntentHelperMojo() = default;
FakeArcIntentHelperMojo::~FakeArcIntentHelperMojo() = default;

bool FakeArcIntentHelperMojo::IsArcAvailable() {
  return true;
}

bool FakeArcIntentHelperMojo::IsRequestUrlHandlerListAvailable() {
  return true;
}

bool FakeArcIntentHelperMojo::IsRequestTextSelectionActionsAvailable() {
  return true;
}

bool FakeArcIntentHelperMojo::RequestUrlHandlerList(
    const std::string& url,
    RequestUrlHandlerListCallback callback) {
  std::move(callback).Run(std::vector<IntentHandlerInfo>());
  return true;
}

bool FakeArcIntentHelperMojo::RequestTextSelectionActions(
    const std::string& text,
    ui::ResourceScaleFactor scale_factor,
    RequestTextSelectionActionsCallback callback) {
  std::move(callback).Run(std::vector<TextSelectionAction>());
  return true;
}
bool FakeArcIntentHelperMojo::HandleUrl(const std::string& url,
                                        const std::string& package_name) {
  return true;
}

bool FakeArcIntentHelperMojo::HandleIntent(const IntentInfo& intent,
                                           const ActivityName& activity) {
  return true;
}

bool FakeArcIntentHelperMojo::AddPreferredPackage(
    const std::string& package_name) {
  return true;
}

}  // namespace arc
