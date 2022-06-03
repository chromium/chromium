// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/public/external_script_controller.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace autofill_assistant {

class CommonDependencies;

class AutofillAssistantImpl : public autofill_assistant::AutofillAssistant {
 public:
  static std::unique_ptr<AutofillAssistantImpl> Create(
      content::BrowserContext* browser_context,
      std::unique_ptr<CommonDependencies> dependencies);

  AutofillAssistantImpl(content::BrowserContext* browser_context,
                        std::unique_ptr<ServiceRequestSender> request_sender,
                        std::unique_ptr<CommonDependencies> dependencies,
                        const GURL& script_server_url);
  ~AutofillAssistantImpl() override;

  AutofillAssistantImpl(const AutofillAssistantImpl&) = delete;
  AutofillAssistantImpl& operator=(const AutofillAssistantImpl&) = delete;

  void GetCapabilitiesByHashPrefix(
      uint32_t hash_prefix_length,
      const std::vector<uint64_t>& hash_prefixes,
      const std::string& intent,
      GetCapabilitiesResponseCallback callback) override;

  std::unique_ptr<ExternalScriptController> CreateExternalScriptController(
      content::WebContents* web_contents,
      ExternalActionDelegate* action_extension_delegate) override;

 private:
  // The `BrowserContext` for which this `AutofillAssistantImpl` was created
  // and which must outlive it.
  const raw_ptr<content::BrowserContext> browser_context_;

  // The request sender responsible for communicating with a remote endpoint.
  std::unique_ptr<ServiceRequestSender> request_sender_;

  // The RPC endpoint to send requests to.
  GURL script_server_url_;

  // Dependencies on client code such as country code or locale.
  std::unique_ptr<CommonDependencies> dependencies_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_IMPL_H_
