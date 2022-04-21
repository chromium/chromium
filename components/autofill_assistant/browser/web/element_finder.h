// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_finder_result_type.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill_assistant {
class BaseElementFinder;
class DevtoolsClient;
class UserData;

// Worker class to find element(s) matching a selector. This will keep entering
// iFrames until the element is found in the last frame, then returns the
// element together with the owning frame. All subsequent operations should
// be performed on that frame.
class ElementFinder : public WebControllerWorker {
 public:
  // |web_contents|, |devtools_client| and |user_data| must be valid for the
  // lifetime of the instance. If |annotate_dom_model_service| is not nullptr,
  // must be valid for the lifetime of the instance.
  ElementFinder(content::WebContents* web_contents,
                DevtoolsClient* devtools_client,
                const UserData* user_data,
                ProcessedActionStatusDetailsProto* log_info,
                AnnotateDomModelService* annotate_dom_model_service,
                const Selector& selector,
                ElementFinderResultType result_type);
  ~ElementFinder() override;

  using Callback =
      base::OnceCallback<void(const ClientStatus&,
                              std::unique_ptr<ElementFinderResult>)>;

  // Finds the element and calls the callback starting from the |start_element|.
  // If it is empty, it will start looking for the Document of the main frame.
  void Start(const ElementFinderResult& start_element, Callback callback);

 private:
  // Updates |log_info_| and calls |callback_| with the |status| and |result|.
  void SendResult(const ClientStatus& status,
                  std::unique_ptr<ElementFinderResult> result);

  void UpdateLogInfo(const ClientStatus& status);

  // Adds a runner to the list and starts it from the |start_element|.
  void AddAndStartRunner(const ElementFinderResult& start_element,
                         std::unique_ptr<BaseElementFinder> runner);
  void OnResult(size_t index,
                const ClientStatus& status,
                std::unique_ptr<ElementFinderResult> result);

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<DevtoolsClient> devtools_client_;
  const raw_ptr<const UserData> user_data_;
  const raw_ptr<ProcessedActionStatusDetailsProto> log_info_;
  const raw_ptr<AnnotateDomModelService> annotate_dom_model_service_;
  const Selector selector_;
  const ElementFinderResultType result_type_;
  Callback callback_;

  std::vector<std::unique_ptr<BaseElementFinder>> runners_;
  std::vector<std::pair<ClientStatus, std::unique_ptr<ElementFinderResult>>>
      results_;
  size_t num_results_ = 0;

  base::WeakPtrFactory<ElementFinder> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
