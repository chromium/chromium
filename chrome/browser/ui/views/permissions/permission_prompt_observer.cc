// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_observer.h"

PermissionPromptObserver::PermissionPromptObserver(
    content::WebContents* contents)
    : content::WebContentsUserData<PermissionPromptObserver>(*contents) {}

PermissionPromptObserver::~PermissionPromptObserver() = default;

void PermissionPromptObserver::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PermissionPromptObserver::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PermissionPromptObserver::NotifyPermissionPromptChanged(
    bool is_showing,
    const gfx::Size& prompt_size) {
  for (auto& observer : observers_) {
    observer.OnPermissionPromptChanged(is_showing, prompt_size);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PermissionPromptObserver);
