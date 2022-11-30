// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NOTIFICATION_HELPER_NOTIFICATION_ACTIVATOR_H_
#define CHROME_NOTIFICATION_HELPER_NOTIFICATION_ACTIVATOR_H_

#include <NotificationActivationCallback.h>
#include <wrl/implements.h>

namespace notification_helper {

// This class is used to create a COM component that exposes the
// INotificationActivationCallback interface, which is required for a Win32
// app to participate with Windows Action Center.
class NotificationActivator
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          INotificationActivationCallback> {
 public:
  NotificationActivator() = default;

  NotificationActivator(const NotificationActivator&) = delete;
  NotificationActivator& operator=(const NotificationActivator&) = delete;

  // Called when a user interacts with a toast in the Windows action center.
  // For the meaning of the input parameters, see
  // https://msdn.microsoft.com/library/windows/desktop/mt643712.aspx
  IFACEMETHODIMP Activate(LPCWSTR app_user_model_id,
                          LPCWSTR invoked_args,
                          const NOTIFICATION_USER_INPUT_DATA* data,
                          ULONG count) override;

 protected:
  ~NotificationActivator() override;
};

}  // namespace notification_helper

#endif  // CHROME_NOTIFICATION_HELPER_NOTIFICATION_ACTIVATOR_H_
