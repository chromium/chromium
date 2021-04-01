// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/test_helper.h"

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "testing/gmock/include/gmock/gmock.h"

MockMediaNotificationItem::MockMediaNotificationItem() = default;
MockMediaNotificationItem::~MockMediaNotificationItem() = default;

base::WeakPtr<MockMediaNotificationItem>
MockMediaNotificationItem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

MockMediaDialogDelegate::MockMediaDialogDelegate() = default;
MockMediaDialogDelegate::~MockMediaDialogDelegate() {
  Close();
}

void MockMediaDialogDelegate::Open(MediaNotificationService* service) {
  ASSERT_TRUE(service);
  service_ = service;
  service_->SetDialogDelegate(this);
}

void MockMediaDialogDelegate::OpenForWebContents(
    MediaNotificationService* service,
    content::WebContents* content) {
  ASSERT_TRUE(service);
  service_ = service;
  service_->SetDialogDelegateForWebContents(this, content);
}

void MockMediaDialogDelegate::Close() {
  if (!service_)
    return;

  service_->SetDialogDelegate(nullptr);
  service_ = nullptr;
}

std::unique_ptr<OverlayMediaNotification> MockMediaDialogDelegate::PopOut(
    const std::string& id,
    gfx::Rect bounds) {
  return std::unique_ptr<OverlayMediaNotification>(PopOutProxy(id, bounds));
}

void MockMediaDialogDelegate::HideMediaDialog() {
  Close();
}
