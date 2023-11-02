// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/autofill_assistant_factory.h"

#include <memory>

#include "components/autofill_assistant/browser/autofill_assistant_impl.h"
#include "components/autofill_assistant/browser/common_dependencies.h"
#include "content/public/browser/browser_context.h"

namespace autofill_assistant {

// static
std::unique_ptr<AutofillAssistant>
AutofillAssistantFactory::CreateForBrowserContext(
    content::BrowserContext* browser_context,
    std::unique_ptr<CommonDependencies> dependencies) {
  return AutofillAssistantImpl::Create(browser_context,
                                       std::move(dependencies));
}

}  // namespace autofill_assistant
