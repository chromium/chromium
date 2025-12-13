// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_contents/browser/guest_contents_handle.h"

#include <map>

#include "base/no_destructor.h"
#include "content/public/browser/unowned_inner_web_contents_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace guest_contents {

namespace {

std::map<GuestId, GuestContentsHandle*>& GetGuestContentsMap() {
  static base::NoDestructor<std::map<GuestId, GuestContentsHandle*>>
      guest_contents_map;
  return *guest_contents_map;
}

}  // namespace

// static
GuestContentsHandle* GuestContentsHandle::CreateForWebContents(
    content::WebContents* web_contents) {
  content::WebContentsUserData<GuestContentsHandle>::CreateForWebContents(
      web_contents);
  return FromWebContents(web_contents);
}

// static
GuestContentsHandle* GuestContentsHandle::FromID(GuestId id) {
  return GetGuestContentsMap().contains(id) ? GetGuestContentsMap()[id]
                                            : nullptr;
}

GuestContentsHandle::GuestContentsHandle(content::WebContents* web_contents)
    : content::WebContentsUserData<GuestContentsHandle>(*web_contents),
      content::WebContentsObserver(web_contents),
      id_(GetNextId()) {
  CHECK(!GetGuestContentsMap().contains(id_));
  GetGuestContentsMap()[id_] = this;
}

GuestContentsHandle::~GuestContentsHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetGuestContentsMap().erase(id_);
}

void GuestContentsHandle::AttachToOuterWebContents(
    content::RenderFrameHost* outer_delegate_frame) {
  CHECK(!web_contents()->GetOuterWebContents())
      << "This WebContents is already attached to an outer WebContents. Hint: "
         "use GuestContentsHandle::DetachFromOuterWebContents() before "
         "re-attaching.";
  content::WebContents* outer_web_contents =
      content::WebContents::FromRenderFrameHost(outer_delegate_frame);
  CHECK(outer_web_contents);
  outer_web_contents->AttachUnownedInnerWebContents(
      content::UnownedInnerWebContentsClient::GetPassKey(), web_contents(),
      outer_delegate_frame);
  CHECK_EQ(web_contents()->GetOuterWebContents(), outer_web_contents);
}

void GuestContentsHandle::DetachFromOuterWebContents() {
  content::WebContents* outer_web_contents =
      web_contents()->GetOuterWebContents();
  if (!outer_web_contents) {
    return;
  }

  outer_web_contents->DetachUnownedInnerWebContents(
      content::UnownedInnerWebContentsClient::GetPassKey(), web_contents());
}

void GuestContentsHandle::WebContentsDestroyed() {
  DetachFromOuterWebContents();
}

GuestId GuestContentsHandle::GetNextId() {
  static GuestId next_id = 0;
  return next_id++;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(GuestContentsHandle);

}  // namespace guest_contents
