// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/web_contents_main_frame_observer.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace dom_distiller {

WebContentsMainFrameObserver::WebContentsMainFrameObserver(
    content::WebContents* web_contents)
    : is_document_loaded_in_main_frame_(false), is_initialized_(false) {
  content::WebContentsObserver::Observe(web_contents);
}

WebContentsMainFrameObserver::~WebContentsMainFrameObserver() {
  CleanUp();
}

void WebContentsMainFrameObserver::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent()) {
    is_document_loaded_in_main_frame_ = true;
  }
}

void WebContentsMainFrameObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  is_document_loaded_in_main_frame_ = false;
  is_initialized_ = true;
}

void WebContentsMainFrameObserver::RenderProcessGone(
    base::TerminationStatus status) {
  CleanUp();
}

void WebContentsMainFrameObserver::CleanUp() {
  content::WebContentsObserver::Observe(nullptr);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsMainFrameObserver)

}  // namespace dom_distiller
