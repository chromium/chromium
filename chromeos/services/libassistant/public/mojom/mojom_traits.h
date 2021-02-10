// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_PUBLIC_MOJOM_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_PUBLIC_MOJOM_MOJOM_TRAITS_H_

#include "chromeos/services/assistant/public/cpp/assistant_notification.h"
#include "chromeos/services/assistant/public/shared/utils.h"
#include "chromeos/services/libassistant/public/mojom/android_app_info.mojom-shared.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<chromeos::libassistant::mojom::AndroidAppInfoDataView,
                    chromeos::assistant::AndroidAppInfo> {
  using AndroidAppInfo = chromeos::assistant::AndroidAppInfo;

  static const std::string& package_name(const AndroidAppInfo& input);
  static int64_t version(const AndroidAppInfo& input);
  static const std::string& localized_app_name(const AndroidAppInfo& input);
  static const std::string& intent(const AndroidAppInfo& input);
  static chromeos::assistant::AppStatus status(const AndroidAppInfo& input);
  static const std::string& action(const AndroidAppInfo& input);

  static bool Read(chromeos::libassistant::mojom::AndroidAppInfoDataView data,
                   AndroidAppInfo* output);
};
template <>
struct EnumTraits<chromeos::libassistant::mojom::AndroidAppStatus,
                  chromeos::assistant::AppStatus> {
  using AppStatus = ::chromeos::assistant::AppStatus;
  using AndroidAppStatus = ::chromeos::libassistant::mojom::AndroidAppStatus;

  static AndroidAppStatus ToMojom(AppStatus input);
  static bool FromMojom(AndroidAppStatus input, AppStatus* output);
};

template <>
struct StructTraits<
    chromeos::libassistant::mojom::AssistantNotificationDataView,
    chromeos::assistant::AssistantNotification> {
  using AssistantNotification = chromeos::assistant::AssistantNotification;

  static const std::string& server_id(const AssistantNotification& input);
  static const std::string& consistency_token(
      const AssistantNotification& input);
  static const std::string& opaque_token(const AssistantNotification& input);
  static const std::string& grouping_key(const AssistantNotification& input);
  static const std::string& obfuscated_gaia_id(
      const AssistantNotification& input);

  static bool Read(
      chromeos::libassistant::mojom::AssistantNotificationDataView data,
      AssistantNotification* output);
};

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_PUBLIC_MOJOM_MOJOM_TRAITS_H_
