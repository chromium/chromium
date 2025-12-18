// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_immersive_web_view.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"

ReadAnythingImmersiveWebView::ReadAnythingImmersiveWebView(
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        contents_wrapper,
    ReadAnythingOpenTrigger trigger)
    : contents_wrapper_(std::move(contents_wrapper)), trigger_(trigger) {
  SetWebContents(contents_wrapper_->web_contents());
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  // Calling ReadAnythingImmersiveWebView::ShowUI is not necessary here- the
  // WebUI will call ShowUI when it is ready.
}

ReadAnythingImmersiveWebView::~ReadAnythingImmersiveWebView() = default;

std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
ReadAnythingImmersiveWebView::CloseAndTakeContentsWrapper() {
  SetWebContents(nullptr);  // This is necessary to reset the web contents.
  contents_wrapper_->SetHost(nullptr);
  SetVisible(false);

  // Call OnEntryHidden on the Controller
  auto* read_anything_controller = ReadAnythingControllerGlue::FromWebContents(
                                       contents_wrapper_->web_contents())
                                       ->controller();
  CHECK(read_anything_controller);
  read_anything_controller->OnEntryHidden();

  return std::move(contents_wrapper_);
}

// WebUIContentsWrapper::Host:
// Called by the WebUI on its embedder (this class) when the WebUI is ready to
// be shown.
void ReadAnythingImmersiveWebView::ShowUI() {
  SetVisible(true);
  auto* read_anything_controller = ReadAnythingControllerGlue::FromWebContents(
                                       contents_wrapper_->web_contents())
                                       ->controller();
  CHECK(read_anything_controller);
  read_anything_controller->OnEntryShown(trigger_);
}

// Called by the WebUI on its embedder (this class) when the WebUI is ready to
// be closed.
void ReadAnythingImmersiveWebView::CloseUI() {
  // This currently does not do anything and is never called because the
  // ReadAnythingController is currentlythe one that owns the WebUI by the time
  // it is closed.
}

BEGIN_METADATA(ReadAnythingImmersiveWebView)
END_METADATA
