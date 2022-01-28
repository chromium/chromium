// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/autofill_assistant_factory.h"

#include "components/autofill_assistant/browser/autofill_assistant_impl.h"
#include "components/version_info/channel.h"

namespace autofill_assistant {

// static
std::unique_ptr<AutofillAssistant>
AutofillAssistantFactory::CreateForBrowserContext(
    content::BrowserContext* browser_context,
    version_info::Channel channel,
    const std::string& country_code,
    const std::string& locale) {
  return AutofillAssistantImpl::Create(browser_context, channel, country_code,
                                       locale);
}

}  // namespace autofill_assistant
