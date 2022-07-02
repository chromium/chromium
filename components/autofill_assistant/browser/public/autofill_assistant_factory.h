// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_FACTORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_FACTORY_H_

#include <memory>

#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_context.h"

namespace autofill_assistant {

class CommonDependencies;

// Factory class for creating |AutofillAssistant|.
class AutofillAssistantFactory {
 public:
  // TODO(b/201964911) The |AutofillAssistant::CreateHeadlessScriptController|
  // method ignores the  |channel|, |country_code| and |locale| passed here and
  // instead fetches them directly. Make the treatment between
  // |HeadlessScriptController| and |GetCapabilitiesByHashPrefix| consistent.
  static std::unique_ptr<AutofillAssistant> CreateForBrowserContext(
      content::BrowserContext* browser_context,
      std::unique_ptr<CommonDependencies> dependencies);
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_FACTORY_H_
