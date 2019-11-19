// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;
using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

ConstrainedWebDialogDelegateBase::ConstrainedWebDialogDelegateBase(
    content::BrowserContext* browser_context,
    std::unique_ptr<WebDialogDelegate> web_dialog_delegate,
    std::unique_ptr<WebDialogWebContentsDelegate> tab_delegate)
    : WebDialogWebContentsDelegate(
          browser_context,
          std::make_unique<ChromeWebContentsHandler>()),
      web_dialog_delegate_(std::move(web_dialog_delegate)),
      closed_via_webui_(false) {
  DCHECK(web_dialog_delegate_);
  web_contents_holder_ =
      WebContents::Create(WebContents::CreateParams(browser_context));
  web_contents_ = web_contents_holder_.get();
  WebContentsObserver::Observe(web_contents_);
  zoom::ZoomController::CreateForWebContents(web_contents_);
  if (tab_delegate) {
    override_tab_delegate_ = std::move(tab_delegate);
    web_contents_->SetDelegate(override_tab_delegate_.get());
  } else {
    web_contents_->SetDelegate(this);
  }
  blink::mojom::RendererPreferences* prefs =
      web_contents_->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, Profile::FromBrowserContext(browser_context));

  web_contents_->SyncRendererPrefs();

  // Set |this| as a delegate so the ConstrainedWebDialogUI can retrieve it.
  ConstrainedWebDialogUI::SetConstrainedDelegate(web_contents_, this);

  web_contents_->GetController().LoadURL(
      web_dialog_delegate_->GetDialogContentURL(), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
}

ConstrainedWebDialogDelegateBase::~ConstrainedWebDialogDelegateBase() {
  if (web_contents_) {
    // Remove reference to |this| in the WebContent since it will becomes
    // invalid and the lifetime of the WebContent may exceed the one of this
    // object.
    ConstrainedWebDialogUI::ClearConstrainedDelegate(web_contents_);
  }
}

const WebDialogDelegate*
    ConstrainedWebDialogDelegateBase::GetWebDialogDelegate() const {
  return web_dialog_delegate_.get();
}

WebDialogDelegate*
    ConstrainedWebDialogDelegateBase::GetWebDialogDelegate() {
  return web_dialog_delegate_.get();
}

void ConstrainedWebDialogDelegateBase::OnDialogCloseFromWebUI() {
  closed_via_webui_ = true;
  CloseContents(web_contents_);
}

bool ConstrainedWebDialogDelegateBase::closed_via_webui() const {
  return closed_via_webui_;
}

std::unique_ptr<content::WebContents>
ConstrainedWebDialogDelegateBase::ReleaseWebContents() {
  return std::move(web_contents_holder_);
}

gfx::NativeWindow ConstrainedWebDialogDelegateBase::GetNativeDialog() {
  NOTREACHED();
  return nullptr;
}

WebContents* ConstrainedWebDialogDelegateBase::GetWebContents() {
  return web_contents_;
}

bool ConstrainedWebDialogDelegateBase::HandleKeyboardEvent(
    content::WebContents* source,
    const NativeWebKeyboardEvent& event) {
  return false;
}

gfx::Size ConstrainedWebDialogDelegateBase::GetConstrainedWebDialogMinimumSize()
    const {
  NOTREACHED();
  return gfx::Size();
}

gfx::Size ConstrainedWebDialogDelegateBase::GetConstrainedWebDialogMaximumSize()
    const {
  NOTREACHED();
  return gfx::Size();
}

gfx::Size
ConstrainedWebDialogDelegateBase::GetConstrainedWebDialogPreferredSize() const {
  NOTREACHED();
  return gfx::Size();
}

void ConstrainedWebDialogDelegateBase::WebContentsDestroyed() {
  web_contents_ = nullptr;
}

void ConstrainedWebDialogDelegateBase::ResizeToGivenSize(
    const gfx::Size size) {
  NOTREACHED();
}
