// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(DocumentPipHost);

DocumentPipHost::DocumentPipHost(
    content::WebContents* opener_web_contents,
    std::unique_ptr<content::WebContents> child_web_contents,
    blink::mojom::PictureInPictureWindowOptions pip_options)
    : content::WebContentsUserData<DocumentPipHost>(*opener_web_contents),
      content::WebContentsObserver(opener_web_contents),
      child_web_contents_(std::move(child_web_contents)),
      pip_options_(std::move(pip_options)) {
  CHECK(child_web_contents_);

  child_web_contents_->SetDelegate(this);
}

DocumentPipHost::~DocumentPipHost() {
  ClosePipWindow();
}

void DocumentPipHost::CreatePipWidget() {
  // Avoid creating a second widget if one already exists.
  if (widget_) {
    return;
  }

  widget_delegate_ = std::make_unique<views::WidgetDelegate>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = widget_delegate_.get();
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;
  // TODO(jiayuchen): Remove once DocumentPipWidgetDelegate introduced
  // and sets `params.native_widget` to a desktop native widget
  // (e.g. BrowserNativeWidgetAura / BrowserNativeWidgetMac), mirroring how a
  // real browser window is created. Until then we use a bare
  // views::WidgetDelegate, so `views::Widget::Init` falls through to the
  // ViewsDelegate-selected NativeWidget. On Aura that requires either
  // `params.parent` or `params.context` (see NativeWidgetAura::InitNativeWidget
  // DCHECK), so point at the opener's top-level native window to pin the PiP
  // window to the same display as the opener.
  params.context = web_contents()->GetTopLevelNativeWindow();
  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  // Intercept external close paths (OS close button, DialogDelegate, etc.) so
  // they route through our teardown logic.
  widget_->MakeCloseSynchronous(base::BindOnce(
      &DocumentPipHost::OnWidgetCloseRequested, base::Unretained(this)));
}

Profile* DocumentPipHost::GetProfile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

content::WebContents* DocumentPipHost::GetOpenerWebContents() {
  return web_contents();
}

content::WebContents* DocumentPipHost::GetChildWebContents() {
  return child_web_contents_.get();
}

views::Widget* DocumentPipHost::GetWidget() {
  return widget_.get();
}

const blink::mojom::PictureInPictureWindowOptions&
DocumentPipHost::GetPipOptions() const {
  return pip_options_;
}

void DocumentPipHost::PrimaryPageChanged(content::Page& page) {
  // The opener navigated to a new primary page; close the PiP window.
  ClosePipWindow();
}

blink::mojom::DisplayMode DocumentPipHost::GetDisplayMode(
    const content::WebContents* web_contents) {
  return blink::mojom::DisplayMode::kPictureInPicture;
}

void DocumentPipHost::CloseContents(content::WebContents* source) {
  // The child WebContents requested closure. Tear down the PiP window and
  // child, but keep DocumentPipHost alive on the opener.
  ClosePipWindow();
}

void DocumentPipHost::ClosePipWindow() {
  if (child_web_contents_) {
    child_web_contents_->SetDelegate(nullptr);
  }
  // CLIENT_OWNS_WIDGET: synchronously destroy widget then child WebContents.
  widget_.reset();
  widget_delegate_.reset();
  child_web_contents_.reset();
}

void DocumentPipHost::OnWidgetCloseRequested(
    views::Widget::ClosedReason reason) {
  ClosePipWindow();
}
