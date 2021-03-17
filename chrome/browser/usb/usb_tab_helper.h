// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_TAB_HELPER_H_
#define CHROME_BROWSER_USB_USB_TAB_HELPER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

// Per-tab owner of USB services provided to render frames within that tab.
class UsbTabHelper : public content::WebContentsUserData<UsbTabHelper> {
 public:
  static UsbTabHelper* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  ~UsbTabHelper() override;

  void IncrementConnectionCount();
  void DecrementConnectionCount();
  bool IsDeviceConnected() const;

 private:
  explicit UsbTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<UsbTabHelper>;

  void NotifyIsDeviceConnectedChanged(bool is_device_connected) const;

  // Initially no device is connected, type int is used as there can be many
  // devices connected to single UsbTabHelper.
  int device_connection_count_ = 0;

  content::WebContents* web_contents_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(UsbTabHelper);
};

#endif  // CHROME_BROWSER_USB_USB_TAB_HELPER_H_
