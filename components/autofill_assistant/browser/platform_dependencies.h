// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PLATFORM_DEPENDENCIES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PLATFORM_DEPENDENCIES_H_

#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Interface for platform delegates that provide dependencies to the starter.
//
// This interface contains all methods which require a platform-specific
// implementation.
class PlatformDependencies {
 public:
  virtual ~PlatformDependencies();

  virtual bool IsCustomTab(const content::WebContents& web_contents) const = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PLATFORM_DEPENDENCIES_H_
