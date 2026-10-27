// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/content/origin_gating_tab_controller.h"

#include "base/check.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "content/public/browser/web_contents.h"

namespace origin_gating {

OriginGatingTabController::OriginGatingTabController(
    content::WebContents* web_contents,
    OriginGatingService& service)
    : content::WebContentsUserData<OriginGatingTabController>(*web_contents),
      content::WebContentsObserver(web_contents),
      service_(service) {}

OriginGatingTabController::~OriginGatingTabController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OriginGatingTabController::SetCheckerId(CheckerId checker_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!checker_id.is_null());
  if (!GetChecker() && service_->GetChecker(checker_id)) {
    checker_id_ = checker_id;
  }
}

void OriginGatingTabController::ClearCheckerId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  checker_id_ = CheckerId();
}

OriginGatingChecker* OriginGatingTabController::GetChecker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (checker_id_.is_null()) {
    return nullptr;
  }
  OriginGatingChecker* checker = service_->GetChecker(checker_id_);
  if (!checker) {
    ClearCheckerId();
  }
  return checker;
}

void OriginGatingTabController::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    ui::mojom::WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PropagateToNewContents(new_contents);
}

void OriginGatingTabController::AboutToBeDiscarded(
    content::WebContents* new_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PropagateToNewContents(new_contents);
}

void OriginGatingTabController::PropagateToNewContents(
    content::WebContents* new_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!new_contents || !GetChecker()) {
    return;
  }
  OriginGatingTabController::GetOrCreateForWebContents(new_contents, *service_)
      ->SetCheckerId(checker_id());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OriginGatingTabController);

}  // namespace origin_gating
