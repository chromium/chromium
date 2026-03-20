// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/frozen_update/frozen_update_notification.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/experiences/frozen_update/frozen_update_gpu_list_autogen.h"
#include "chromeos/constants/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace {

using ::l10n_util::GetStringUTF16;

}  // namespace

// static
bool FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(
    PrefService& prefs) {
  // Feature is disabled.
  if (!base::FeatureList::IsEnabled(features::kShowFrozenUpdateNotification)) {
    return false;
  }

  // Notifications are only for reven devices
  if (!switches::IsRevenBranding()) {
    return false;
  }

  // Do not show frozen update notification if this device is managed
  // by an enterprise user.
  if (InstallAttributes::Get()->IsEnterpriseManaged()) {
    return false;
  }

  // Do not show if subject to parental controls.
  if (supervised_user::IsSubjectToParentalControls(prefs)) {
    return false;
  }

  // Do not show if notification has already been dismissed.
  if (prefs.GetBoolean(prefs::kFrozenUpdateNotificationDismissed)) {
    return false;
  }

  return true;
}

// static:
void FrozenUpdateNotification::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFrozenUpdateNotificationDismissed,
                                false);
}

FrozenUpdateNotification::FrozenUpdateNotification(PrefService& prefs)
    : prefs_(prefs) {}

FrozenUpdateNotification::~FrozenUpdateNotification() = default;

void FrozenUpdateNotification::MaybeShowNotification() {
  unsigned int vendor;
  unsigned int device;
  if (test_vendor_ && test_device_) {
    vendor = *test_vendor_;
    device = *test_device_;
  } else {
    content::GpuDataManager* manager = content::GpuDataManager::GetInstance();
    CHECK(manager);

    auto gpu_info = manager->GetGPUInfo();
    vendor = gpu_info.gpu.vendor_id;
    device = gpu_info.gpu.device_id;
  }

  auto entries = GetFrozenGpuEntries();
  for (const auto& entry : entries) {
    if (vendor == entry.vendor && device == entry.device) {
      ShowNotification();
      break;
    }
  }
}

void FrozenUpdateNotification::ShowNotification() {
  message_center::RichNotificationData data;
  ash::SystemNotificationBuilder notification_builder;

  DCHECK_EQ(kMoreInfoButtonIndex, data.buttons.size());
  data.buttons.emplace_back(GetStringUTF16(IDS_LEARN_MORE));
  DCHECK_EQ(kDismissButtonIndex, data.buttons.size());
  data.buttons.emplace_back(GetStringUTF16(IDS_FROZEN_UPDATE_DISMISS_BUTTON));

  // Notifies user that updates will no longer occur after the final update.
  notification_builder.SetTitleId(IDS_FROZEN_UPDATE_NOTIFICATION_TITLE)
      .SetMessageId(IDS_FROZEN_UPDATE_NOTIFICATION_FROZEN_UPDATE)
      .SetCatalogName(NotificationCatalogName::kFrozenUpdateNotification)
      .SetSmallImage(vector_icons::kNotificationEndOfSupportIcon);

  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          notification_builder
              .SetId(FrozenUpdateNotification::kFrozenUpdateNotificationId)
              .SetOriginUrl(
                  GURL(FrozenUpdateNotification::kFrozenUpdateNotificationId))
              .SetOptionalFields(data)
              .SetDelegate(base::MakeRefCounted<
                           message_center::ThunkNotificationDelegate>(
                  weak_ptr_factory_.GetWeakPtr()))
              .Build(false /* keep_timestamp */)));
}

void FrozenUpdateNotification::Close(bool by_user) {
  if (by_user) {
    prefs_->SetBoolean(prefs::kFrozenUpdateNotificationDismissed, true);
  }
}

void FrozenUpdateNotification::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!button_index) {
    return;
  }

  switch (*button_index) {
    case kMoreInfoButtonIndex: {
      const GURL url(chromeos::kFrozenUpdateNotificationURL);

      // Show more info link.
      NewWindowDelegate::GetInstance()->OpenUrl(
          url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
          NewWindowDelegate::Disposition::kNewForegroundTab);
      break;
    }
    case kDismissButtonIndex:
      // Do nothing, on any interaction we will set the dismiss pref.
      break;
  }

  prefs_->SetBoolean(prefs::kFrozenUpdateNotificationDismissed, true);
  message_center::MessageCenter::Get()->RemoveNotification(
      kFrozenUpdateNotificationId, /*by_user=*/false);
}

void FrozenUpdateNotification::OverrideGpuForTesting(unsigned int vendor,
                                                     unsigned int device) {
  test_vendor_ = vendor;
  test_device_ = device;
}

}  // namespace ash
