// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_BROWSER_APP_INSTANCE_REGISTRY_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_BROWSER_APP_INSTANCE_REGISTRY_MOJOM_TRAITS_H_

#include "components/services/app_service/public/cpp/browser_app_instance_update.h"
#include "components/services/app_service/public/cpp/browser_window_instance_update.h"

#include "chromeos/crosapi/mojom/browser_app_instance_registry.mojom.h"

namespace mojo {

template <>
struct StructTraits<crosapi::mojom::BrowserWindowInstanceUpdateDataView,
                    apps::BrowserWindowInstanceUpdate> {
  static bool Read(crosapi::mojom::BrowserWindowInstanceUpdateDataView input,
                   apps::BrowserWindowInstanceUpdate* output);

  static const base::UnguessableToken& id(
      const apps::BrowserWindowInstanceUpdate& update) {
    return update.id;
  }

  static const std::string& window_id(
      const apps::BrowserWindowInstanceUpdate& update) {
    return update.window_id;
  }

  static bool is_active(const apps::BrowserWindowInstanceUpdate& update) {
    return update.is_active;
  }

  static uint32_t browser_session_id(
      const apps::BrowserWindowInstanceUpdate& update) {
    return update.browser_session_id;
  }

  static uint32_t restored_browser_session_id(
      const apps::BrowserWindowInstanceUpdate& update) {
    return update.restored_browser_session_id;
  }

  static bool is_incognito(const apps::BrowserWindowInstanceUpdate& update) {
    return update.is_incognito;
  }

  static uint64_t lacros_profile_id(
      const apps::BrowserWindowInstanceUpdate& update) {
    return update.lacros_profile_id;
  }
};

template <>
struct StructTraits<crosapi::mojom::BrowserAppInstanceUpdateDataView,
                    apps::BrowserAppInstanceUpdate> {
  static bool Read(crosapi::mojom::BrowserAppInstanceUpdateDataView input,
                   apps::BrowserAppInstanceUpdate* output);

  static const base::UnguessableToken& id(
      const apps::BrowserAppInstanceUpdate& update) {
    return update.id;
  }

  static apps::BrowserAppInstanceUpdate::Type type(
      const apps::BrowserAppInstanceUpdate& update) {
    return update.type;
  }

  static const std::string& app_id(
      const apps::BrowserAppInstanceUpdate& update) {
    return update.app_id;
  }

  static const std::string& window_id(
      const apps::BrowserAppInstanceUpdate& update) {
    return update.window_id;
  }

  static const std::string& title(
      const apps::BrowserAppInstanceUpdate& update) {
    return update.title;
  }

  static bool is_browser_active(const apps::BrowserAppInstanceUpdate& update) {
    return update.is_browser_active;
  }

  static bool is_web_contents_active(
      const apps::BrowserAppInstanceUpdate& update) {
    return update.is_web_contents_active;
  }

  static uint32_t browser_session_id(
      const apps::BrowserAppInstanceUpdate& update) {
    return update.browser_session_id;
  }

  static uint32_t restored_browser_session_id(
      const apps::BrowserAppInstanceUpdate& update) {
    return update.restored_browser_session_id;
  }
};

template <>
struct EnumTraits<crosapi::mojom::BrowserAppInstanceType,
                  apps::BrowserAppInstanceUpdate::Type> {
  static crosapi::mojom::BrowserAppInstanceType ToMojom(
      apps::BrowserAppInstanceUpdate::Type input);
  static bool FromMojom(crosapi::mojom::BrowserAppInstanceType input,
                        apps::BrowserAppInstanceUpdate::Type* output);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_BROWSER_APP_INSTANCE_REGISTRY_MOJOM_TRAITS_H_
