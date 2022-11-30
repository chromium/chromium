// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TAB_HELPER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TAB_HELPER_H_

#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/autofill_assistant/browser/platform_dependencies.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
// Creates the starter instance for the |web_contents|.
void CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<CommonDependencies> common_dependencies,
    std::unique_ptr<PlatformDependencies> platform_dependencies);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TAB_HELPER_H_
