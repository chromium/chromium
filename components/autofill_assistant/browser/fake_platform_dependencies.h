// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_PLATFORM_DEPENDENCIES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_PLATFORM_DEPENDENCIES_H_

#include "components/autofill_assistant/browser/platform_dependencies.h"

namespace autofill_assistant {

class FakePlatformDependencies : public PlatformDependencies {
 public:
  FakePlatformDependencies();
  ~FakePlatformDependencies() override;

  // From PlatformDependencies:
  bool IsCustomTab(const content::WebContents& web_contents) const override;

  // Intentionally public to allow tests direct access.
  bool is_custom_tab_ = false;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_PLATFORM_DEPENDENCIES_H_
