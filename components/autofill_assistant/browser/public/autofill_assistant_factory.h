// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_FACTORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_FACTORY_H_

#include <memory>

#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/version_info/channel.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace autofill_assistant {

class CommonDependencies;

// Factory class for creating |AutofillAssistant|.
class AutofillAssistantFactory {
 public:
  static std::unique_ptr<AutofillAssistant> CreateForBrowserContext(
      content::BrowserContext* browser_context,
      std::unique_ptr<CommonDependencies> dependencies);
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_FACTORY_H_
