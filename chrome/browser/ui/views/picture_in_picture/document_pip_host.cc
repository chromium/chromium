// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_widget_delegate.h"
#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_tucker.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/views/widget/widget.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(DocumentPipHost);

DocumentPipHost::DocumentPipHost(content::WebContents* opener_web_contents)
    : content::WebContentsUserData<DocumentPipHost>(*opener_web_contents),
      content::WebContentsObserver(opener_web_contents) {}

DocumentPipHost::~DocumentPipHost() {
  ClosePipWindow();
}

void DocumentPipHost::CreatePipWidget(
    std::unique_ptr<content::WebContents> child_web_contents,
    blink::mojom::PictureInPictureWindowOptions pip_options) {
  // Avoid creating a second widget if one already exists.
  if (widget_) {
    return;
  }

  CHECK(child_web_contents);
  pip_options_ = std::move(pip_options);

  child_web_contents->SetDelegate(this);

  widget_delegate_ = std::make_unique<DocumentPipWidgetDelegate>(
      this, std::move(child_web_contents));

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  // The Widget stores `delegate` as a raw pointer. Ownership stays with
  // `widget_delegate_` because we use CLIENT_OWNS_WIDGET without
  // SetOwnedByWidget(); the Widget will not delete the delegate.
  params.delegate = widget_delegate_.get();
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;
  params.bounds = gfx::Rect(pip_options_.width, pip_options_.height);

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  // Intercept external close paths (OS close button, DialogDelegate, etc.) so
  // they route through our teardown logic.
  // Safety: `this` owns `widget_` via unique_ptr, so the widget (and its
  // close callback) cannot outlive this host.
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
  // The child is owned by the DocumentPipContentsView (a views::WebView)
  // inside the widget.
  if (widget_delegate_) {
    if (auto* contents_view = widget_delegate_->GetDocumentPipContentsView()) {
      return contents_view->web_contents();
    }
  }
  return nullptr;
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
  // Clear the child's delegate before tearing down, since the host set itself
  // as delegate in CreatePipWidget().
  content::WebContents* child = GetChildWebContents();
  if (child) {
    child->SetDelegate(nullptr);
  }

  // Destroy the tucker before the widget, since it references the widget.
  tucker_.reset();
  is_tucking_forced_ = false;

  // CLIENT_OWNS_WIDGET: synchronously destroy the widget. This tears down the
  // view tree → DocumentPipContentsView (the WebView) → child WebContents.
  // The widget references `widget_delegate_` by raw pointer, so destroy the
  // widget first, then the delegate.
  widget_.reset();
  widget_delegate_.reset();
}

void DocumentPipHost::OnWidgetCloseRequested(
    views::Widget::ClosedReason reason) {
  ClosePipWindow();
}

void DocumentPipHost::SetForcedTucking(bool tuck) {
  if (!tucker_ && widget_) {
    tucker_ = std::make_unique<PictureInPictureTucker>(*widget_);
  }
  is_tucking_forced_ = tuck;

  // Attempting to tuck our Widget before it's been shown causes issues since
  // it may be still adjusting its bounds. Once visible, tucking will be
  // enforced.
  if (widget_ && widget_->IsVisible()) {
    if (is_tucking_forced_) {
      tucker_->Tuck();
    } else {
      tucker_->Untuck();
    }
  }
}

#if BUILDFLAG(IS_MAC)
void DocumentPipHost::OnAnyBrowserEnteredFullscreen() {
  if (widget_) {
    widget_->MoveToActiveFullscreenSpace();
  }
}
#endif
