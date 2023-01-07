// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_DEVTOOLS_WRAPPER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_DEVTOOLS_WRAPPER_H_

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Wraps a devtools client for js flow execution. After the first call to
// GetDevtoolsAndMaybeInit the js flow library can not be changed anymore.
// NOTE: This class is tested in ScriptExecutorBrowserTest and
// JsFlowExecutorImplBrowserTest.
class JsFlowDevtoolsWrapper {
 public:
  // Creates and owns the web contents. The devtools client and web contents are
  // lazily initialized on the first call to GetDevtoolsAndMaybeInit.
  explicit JsFlowDevtoolsWrapper(content::BrowserContext* browser_context);

  // Does not own the web contents.
  explicit JsFlowDevtoolsWrapper(content::WebContents* web_contents);

  ~JsFlowDevtoolsWrapper();
  JsFlowDevtoolsWrapper(const JsFlowDevtoolsWrapper&) = delete;
  JsFlowDevtoolsWrapper& operator=(const JsFlowDevtoolsWrapper&) = delete;

  // The first call to this function starts the initialization. Afterwards the
  // callback is called immediately with the results from the initialization.
  //
  // If an error occurred during initialization status.ok() is false. The
  // devtools client and isolated world context id are only guaranteed to be
  // valid if status.ok() is true.
  void GetDevtoolsAndMaybeInit(
      base::OnceCallback<void(const ClientStatus& status,
                              DevtoolsClient* devtools_client,
                              int isolated_world_context_id)> callback);

  // Sets the js flow library. Can only be called before the first call to
  // GetDevtoolsAndMaybeInit.
  void SetJsFlowLibrary(const std::string& js_flow_library);

 private:
  // Creates the web contents and devtools client if the browser context
  // constructor was used.
  void MabyeCreateDevtoolsClient();

  void OnGetFrameTree(const DevtoolsClient::ReplyStatus& reply_status,
                      std::unique_ptr<page::GetFrameTreeResult> result);

  void OnIsolatedWorldCreated(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<page::CreateIsolatedWorldResult> result);

  void OnJsFlowLibraryEvaluated(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::EvaluateResult> result);

  // Called when the devtools environment has finished initializing. Guaranteed
  // to be called after the first call to GetDevtoolsAndMaybeInit (eventually).
  void FinishInit();

  // True if the wrapper is currently initializing (after
  // GetDevtoolsAndMaybeInit was called but
  // before FinishInit was called).
  bool InitStarted();
  // True after FinishInit was called.
  bool InitDone();

  raw_ptr<content::BrowserContext> browser_context_;
  std::string js_flow_library_;

  // Only set for the browser context constructor. Lazily instantiated.
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<DevtoolsClient> devtools_client_;

  // Set after the wrapper has finished initialization.
  ClientStatus init_status_ = ClientStatus(ACTION_APPLIED);
  int isolated_world_context_id_ = -1;

  base::OnceCallback<void(const ClientStatus& status,
                          DevtoolsClient* devtools_client,
                          int isolated_world_context_id)>
      callback_;

  base::WeakPtrFactory<JsFlowDevtoolsWrapper> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_DEVTOOLS_WRAPPER_H_
