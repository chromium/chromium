// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/permission_bubble/permission_bubble_test_util.h"

#include "components/permissions/permission_request.h"
#include "url/gurl.h"

TestPermissionBubbleViewDelegate::TestPermissionBubbleViewDelegate() = default;

TestPermissionBubbleViewDelegate::~TestPermissionBubbleViewDelegate() = default;

const std::vector<std::unique_ptr<permissions::PermissionRequest>>&
TestPermissionBubbleViewDelegate::Requests() {
  return requests_;
}

GURL TestPermissionBubbleViewDelegate::GetRequestingOrigin() const {
  return requests_.front()->requesting_origin();
}

GURL TestPermissionBubbleViewDelegate::GetEmbeddingOrigin() const {
  return GURL("https://embedder.example.com");
}

std::optional<permissions::PermissionUiSelector::QuietUiReason>
TestPermissionBubbleViewDelegate::ReasonForUsingQuietUi() const {
  return std::nullopt;
}

bool TestPermissionBubbleViewDelegate::ShouldCurrentRequestUseQuietUI() const {
  return false;
}

bool TestPermissionBubbleViewDelegate::
    ShouldDropCurrentRequestIfCannotShowQuietly() const {
  return false;
}

bool TestPermissionBubbleViewDelegate::WasCurrentRequestAlreadyDisplayed() {
  return false;
}

bool TestPermissionBubbleViewDelegate::RecreateView() {
  return false;
}

const permissions::PermissionPrompt*
TestPermissionBubbleViewDelegate::GetCurrentPrompt() const {
  return nullptr;
}

content::WebContents*
TestPermissionBubbleViewDelegate::GetAssociatedWebContents() {
  return nullptr;
}

base::WeakPtr<permissions::PermissionPrompt::Delegate>
TestPermissionBubbleViewDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
