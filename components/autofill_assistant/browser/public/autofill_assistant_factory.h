// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_FACTORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_FACTORY_H_

#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_context.h"

namespace autofill_assistant {
// Creates an instance of |AutofillAssistant|.
class AutofillAssistantFactory {
 public:
  // TODO(b/201964911) The |AutofillAssistant::CreateExternalScriptController|
  // method ignores the  |channel|, |country_code| and |locale| passed here and
  // instead fetches them directly. We should remove these values from this
  // signature and fetch them directly for
  // |AutofillAssistant::GetCapabilitiesByHashPrefix| as well.
  static std::unique_ptr<AutofillAssistant> CreateForBrowserContext(
      content::BrowserContext* browser_context,
      version_info::Channel channel,
      const std::string& country_code,
      const std::string& locale);
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_FACTORY_H_
