// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MICROPHONE_MUTE_NOTIFICATION_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_MICROPHONE_MUTE_NOTIFICATION_DELEGATE_IMPL_H_

#include <string>

#include "ash/public/cpp/microphone_mute_notification_delegate.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {
class AppCapabilityAccessCache;
class AppRegistryCache;
}  // namespace apps

class MicrophoneMuteNotificationDelegateImpl
    : public ash::MicrophoneMuteNotificationDelegate {
 public:
  MicrophoneMuteNotificationDelegateImpl();
  MicrophoneMuteNotificationDelegateImpl(
      const MicrophoneMuteNotificationDelegateImpl&) = delete;
  MicrophoneMuteNotificationDelegateImpl& operator=(
      const MicrophoneMuteNotificationDelegateImpl&) = delete;
  ~MicrophoneMuteNotificationDelegateImpl() override;

  // ash::MicrophoneMuteNotificationDelegate
  absl::optional<std::u16string> GetAppAccessingMicrophone() override;

 private:
  friend class MicrophoneMuteNotificationDelegateTest;

  absl::optional<std::u16string> GetAppAccessingMicrophone(
      apps::AppCapabilityAccessCache* capability_cache,
      apps::AppRegistryCache* registry_cache);

  base::WeakPtrFactory<MicrophoneMuteNotificationDelegateImpl>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_MICROPHONE_MUTE_NOTIFICATION_DELEGATE_IMPL_H_
