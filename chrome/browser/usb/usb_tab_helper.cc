// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_tab_helper.h"

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

// static
UsbTabHelper* UsbTabHelper::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  UsbTabHelper* tab_helper = FromWebContents(web_contents);
  if (!tab_helper) {
    CreateForWebContents(web_contents);
    tab_helper = FromWebContents(web_contents);
  }
  return tab_helper;
}

UsbTabHelper::~UsbTabHelper() {
  DCHECK_EQ(0, device_connection_count_);
}

UsbTabHelper::UsbTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<UsbTabHelper>(*web_contents) {}

void UsbTabHelper::NotifyIsDeviceConnectedChanged(bool is_device_connected) {
  performance_manager::PageLiveStateDecorator::OnIsConnectedToUSBDeviceChanged(
      &GetWebContents(), is_device_connected);
  GetWebContents().NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
}

void UsbTabHelper::IncrementConnectionCount() {
  device_connection_count_++;
  if (device_connection_count_ == 1)
    NotifyIsDeviceConnectedChanged(/*is_device_connected=*/true);
}

void UsbTabHelper::DecrementConnectionCount() {
  DCHECK_GT(device_connection_count_, 0);
  device_connection_count_--;
  if (device_connection_count_ == 0)
    NotifyIsDeviceConnectedChanged(/*is_device_connected=*/false);
}

bool UsbTabHelper::IsDeviceConnected() const {
  return device_connection_count_ > 0;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UsbTabHelper);
