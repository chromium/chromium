// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_page_controller.h"

#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-microtask-queue.h"

gin::WrapperInfo NetErrorPageController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

NetErrorPageController::Delegate::Delegate() {}
NetErrorPageController::Delegate::~Delegate() {}

// static
void NetErrorPageController::Install(content::RenderFrame* render_frame,
                                     base::WeakPtr<Delegate> delegate) {
  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  gin::Handle<NetErrorPageController> controller = gin::CreateHandle(
      isolate, new NetErrorPageController(delegate));
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context, gin::StringToV8(isolate, "errorPageController"),
            controller.ToV8())
      .ToChecked();
}

bool NetErrorPageController::DownloadButtonClick() {
  return ButtonClick(NetErrorHelperCore::DOWNLOAD_BUTTON);
}

bool NetErrorPageController::ReloadButtonClick() {
  return ButtonClick(NetErrorHelperCore::RELOAD_BUTTON);
}

bool NetErrorPageController::DetailsButtonClick() {
  return ButtonClick(NetErrorHelperCore::MORE_BUTTON);
}

bool NetErrorPageController::TrackEasterEgg() {
  return ButtonClick(NetErrorHelperCore::EASTER_EGG);
}

bool NetErrorPageController::UpdateEasterEggHighScore(int high_score) {
  if (delegate_)
    delegate_->UpdateEasterEggHighScore(high_score);
  return true;
}

bool NetErrorPageController::ResetEasterEggHighScore() {
  if (delegate_)
    delegate_->ResetEasterEggHighScore();
  return true;
}

bool NetErrorPageController::DiagnoseErrorsButtonClick() {
  return ButtonClick(NetErrorHelperCore::DIAGNOSE_ERROR);
}

bool NetErrorPageController::PortalSigninButtonClick() {
  return ButtonClick(NetErrorHelperCore::PORTAL_SIGNIN);
}

bool NetErrorPageController::ButtonClick(NetErrorHelperCore::Button button) {
  if (delegate_)
    delegate_->ButtonPressed(button);

  return true;
}

void NetErrorPageController::LaunchOfflineItem(gin::Arguments* args) {
  if (!delegate_)
    return;
  std::string id;
  std::string name_space;
  if (args->GetNext(&id) && args->GetNext(&name_space))
    delegate_->LaunchOfflineItem(id, name_space);
}

void NetErrorPageController::LaunchDownloadsPage() {
  if (delegate_)
    delegate_->LaunchDownloadsPage();
}

void NetErrorPageController::SavePageForLater() {
  if (delegate_)
    delegate_->SavePageForLater();
}

void NetErrorPageController::CancelSavePage() {
  if (delegate_)
    delegate_->CancelSavePage();
}

void NetErrorPageController::ListVisibilityChanged(bool is_visible) {
  if (delegate_)
    delegate_->ListVisibilityChanged(is_visible);
}

NetErrorPageController::NetErrorPageController(base::WeakPtr<Delegate> delegate)
    : delegate_(delegate) {
}

NetErrorPageController::~NetErrorPageController() {}

gin::ObjectTemplateBuilder NetErrorPageController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<NetErrorPageController>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("downloadButtonClick",
                 &NetErrorPageController::DownloadButtonClick)
      .SetMethod("reloadButtonClick",
                 &NetErrorPageController::ReloadButtonClick)
      .SetMethod("detailsButtonClick",
                 &NetErrorPageController::DetailsButtonClick)
      .SetMethod("diagnoseErrorsButtonClick",
                 &NetErrorPageController::DiagnoseErrorsButtonClick)
      .SetMethod("portalSigninButtonClick",
                 &NetErrorPageController::PortalSigninButtonClick)
      .SetMethod("trackEasterEgg", &NetErrorPageController::TrackEasterEgg)
      .SetMethod("updateEasterEggHighScore",
                 &NetErrorPageController::UpdateEasterEggHighScore)
      .SetMethod("resetEasterEggHighScore",
                 &NetErrorPageController::ResetEasterEggHighScore)
      .SetMethod("launchOfflineItem",
                 &NetErrorPageController::LaunchOfflineItem)
      .SetMethod("launchDownloadsPage",
                 &NetErrorPageController::LaunchDownloadsPage)
      .SetMethod("savePageForLater", &NetErrorPageController::SavePageForLater)
      .SetMethod("cancelSavePage", &NetErrorPageController::CancelSavePage)
      .SetMethod("listVisibilityChanged",
                 &NetErrorPageController::ListVisibilityChanged);
}
