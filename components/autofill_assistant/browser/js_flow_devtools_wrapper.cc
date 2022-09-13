// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_devtools_wrapper.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/js_flow_util.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"

namespace autofill_assistant {

constexpr char kAboutBlankURL[] = "about:blank";

namespace {
std::unique_ptr<DevtoolsClient> CreateDevtoolsClient(
    content::WebContents* web_contents) {
  return std::make_unique<DevtoolsClient>(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents),
      base::FeatureList::IsEnabled(
          autofill_assistant::features::
              kAutofillAssistantFullJsFlowStackTraces));
}
}  // namespace

JsFlowDevtoolsWrapper::JsFlowDevtoolsWrapper(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

JsFlowDevtoolsWrapper::JsFlowDevtoolsWrapper(content::WebContents* web_contents)
    : devtools_client_(CreateDevtoolsClient(web_contents)) {}

void JsFlowDevtoolsWrapper::SetJsFlowLibrary(
    const std::string& js_flow_library) {
  if (InitStarted() || InitDone()) {
    LOG(ERROR) << "The js flow library can't be set after the devtools wrapper "
                  "has started initializing.";
    return;
  }

  js_flow_library_ = js_flow_library;
}

JsFlowDevtoolsWrapper::~JsFlowDevtoolsWrapper() = default;

void JsFlowDevtoolsWrapper::GetDevtoolsAndMaybeInit(
    base::OnceCallback<void(const ClientStatus& status,
                            DevtoolsClient* devtools_client,
                            int isolated_world_context_id)> callback) {
  if (InitDone()) {
    std::move(callback).Run(init_status_, devtools_client_.get(),
                            isolated_world_context_id_);
    return;
  }

  if (InitStarted()) {
    LOG(ERROR) << "Invoked " << __func__ << " while already initializing";
    return;
  }
  callback_ = std::move(callback);

  MabyeCreateDevtoolsClient();
  devtools_client_->GetPage()->GetFrameTree(
      js_flow_util::kMainFrame,
      base::BindOnce(&JsFlowDevtoolsWrapper::OnGetFrameTree,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowDevtoolsWrapper::MabyeCreateDevtoolsClient() {
  if (devtools_client_) {
    return;
  }

  // To execute JS flows we create a new web contents that persists
  // across navigations.
  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(browser_context_));
  // Navigate to a blank page to connect to a frame tree.
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(GURL(kAboutBlankURL)));

  devtools_client_ = CreateDevtoolsClient(web_contents_.get());
}

void JsFlowDevtoolsWrapper::OnGetFrameTree(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<page::GetFrameTreeResult> result) {
  if (!result) {
    LOG(ERROR) << "failed to retrieve frame tree";
    Metrics::RecordJsFlowStartedEvent(
        Metrics::JsFlowStartedEvent::FAILED_TO_GET_FRAME_TREE);
    init_status_ =
        JavaScriptErrorStatus(reply_status, __FILE__, __LINE__, nullptr);
    FinishInit();
    return;
  }
  VLOG(2) << "frame tree retrieved";

  devtools_client_->GetPage()->CreateIsolatedWorld(
      page::CreateIsolatedWorldParams::Builder()
          .SetFrameId(result->GetFrameTree()->GetFrame()->GetId())
          .Build(),
      js_flow_util::kMainFrame,
      base::BindOnce(&JsFlowDevtoolsWrapper::OnIsolatedWorldCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowDevtoolsWrapper::OnIsolatedWorldCreated(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<page::CreateIsolatedWorldResult> result) {
  if (!result) {
    LOG(ERROR) << "failed to create isolated world";
    Metrics::RecordJsFlowStartedEvent(
        Metrics::JsFlowStartedEvent::FAILED_TO_CREATE_ISOLATED_WORLD);
    init_status_ =
        JavaScriptErrorStatus(reply_status, __FILE__, __LINE__, nullptr);
    FinishInit();
    return;
  }
  VLOG(2) << "isolated world created";

  isolated_world_context_id_ = result->GetExecutionContextId();

  // Append the source url.
  const auto js_flow_library = base::StrCat(
      {js_flow_library_, js_flow_util::GetDevtoolsSourceUrlCommentToAppend(
                             UnexpectedErrorInfoProto::JS_FLOW_LIBRARY)});

  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(js_flow_library)
          .SetContextId(isolated_world_context_id_)
          .SetAwaitPromise(true)
          .SetReturnByValue(true)
          .Build(),
      js_flow_util::kMainFrame,
      base::BindOnce(&JsFlowDevtoolsWrapper::OnJsFlowLibraryEvaluated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowDevtoolsWrapper::OnJsFlowLibraryEvaluated(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  std::unique_ptr<base::Value> unused;
  init_status_ = js_flow_util::ExtractFlowReturnValue(
      reply_status, result.get(), unused, /* js_line_offsets= */ {});

  if (init_status_.ok()) {
    VLOG(2) << "JS flow library (length " << js_flow_library_.length()
            << ") evaluated";
  } else {
    LOG(ERROR) << "JS flow library (length " << js_flow_library_.length()
               << ") could not be evaluated";
  }

  FinishInit();
}

void JsFlowDevtoolsWrapper::FinishInit() {
  std::move(callback_).Run(init_status_, devtools_client_.get(),
                           isolated_world_context_id_);
}

bool JsFlowDevtoolsWrapper::InitStarted() {
  return !callback_.is_null();
}

bool JsFlowDevtoolsWrapper::InitDone() {
  return isolated_world_context_id_ != -1 || !init_status_.ok();
}

}  // namespace autofill_assistant
