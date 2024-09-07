// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/arc/arc_custom_tab_modal_dialog_host.h"

#include <utility>

#include "components/arc/intent_helper/custom_tab.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

ArcCustomTabModalDialogHost::ArcCustomTabModalDialogHost(
    std::unique_ptr<arc::CustomTab> custom_tab,
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      custom_tab_(std::move(custom_tab)),
      web_contents_(web_contents) {
  // Attach any required WebContents helpers. Browser tabs automatically get
  // them attached in TabHelpers::AttachTabHelpers.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents_);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
      ->SetDelegate(this);
}

ArcCustomTabModalDialogHost::~ArcCustomTabModalDialogHost() {
  for (auto& observer : observer_list_)
    observer.OnHostDestroying();

  // |web_contents_| is deleted by the base class, so there's no need to call
  // WebContentsModalDialogManager::SetDelegate(nullptr)
}

void ArcCustomTabModalDialogHost::PrimaryMainFrameWasResized(
    bool width_changed) {
  for (auto& observer : observer_list_)
    observer.OnPositionRequiresUpdate();
}

web_modal::WebContentsModalDialogHost*
ArcCustomTabModalDialogHost::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView ArcCustomTabModalDialogHost::GetHostView() const {
  return custom_tab_->GetHostView();
}

gfx::Point ArcCustomTabModalDialogHost::GetDialogPosition(
    const gfx::Size& size) {
  return web_contents_->GetViewBounds().origin();
}

gfx::Size ArcCustomTabModalDialogHost::GetMaximumDialogSize() {
  return web_contents_->GetViewBounds().size();
}

void ArcCustomTabModalDialogHost::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ArcCustomTabModalDialogHost::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}
