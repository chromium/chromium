// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_
#define COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "components/facilitated_payments/core/browser/device_delegate.h"
#include "content/public/browser/web_contents.h"

namespace payments::facilitated {

// Android implementation of `DeviceDelegate`.
class DeviceDelegateAndroid : public DeviceDelegate {
 public:
  explicit DeviceDelegateAndroid(content::WebContents* web_contents);
  DeviceDelegateAndroid(const DeviceDelegateAndroid&) = delete;
  DeviceDelegateAndroid& operator=(const DeviceDelegateAndroid&) = delete;
  ~DeviceDelegateAndroid() override;

  // Returns true if Google Wallet is installed, and its version supports Pix
  // account linking.
  bool IsPixAccountLinkingSupported() const override;

  // Opens the Pix account linking page in Google Wallet.
  void LaunchPixAccountLinkingPage() override;

 private:
  base::WeakPtr<content::WebContents> web_contents_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_
