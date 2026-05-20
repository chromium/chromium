// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_observer.h"

EmbeddedPermissionPromptObserver::EmbeddedPermissionPromptObserver(
    content::WebContents* contents)
    : content::WebContentsUserData<EmbeddedPermissionPromptObserver>(
          *contents) {}

EmbeddedPermissionPromptObserver::~EmbeddedPermissionPromptObserver() = default;

void EmbeddedPermissionPromptObserver::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EmbeddedPermissionPromptObserver::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
void EmbeddedPermissionPromptObserver::NotifyEmbeddedPermissionPromptChanged(
    bool is_showing,
    const gfx::Size& prompt_size) {
  for (auto& observer : observers_) {
    observer.OnEmbeddedPermissionPromptChanged(is_showing, prompt_size);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(EmbeddedPermissionPromptObserver);
