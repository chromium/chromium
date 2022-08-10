// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_platform_dependencies.h"

namespace autofill_assistant {

FakePlatformDependencies::FakePlatformDependencies() = default;
FakePlatformDependencies::~FakePlatformDependencies() = default;

bool FakePlatformDependencies::IsCustomTab(
    const content::WebContents& web_contents) const {
  return is_custom_tab_;
}

}  // namespace autofill_assistant
