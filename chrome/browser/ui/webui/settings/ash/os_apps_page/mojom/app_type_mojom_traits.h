// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_APPS_PAGE_MOJOM_APP_TYPE_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_APPS_PAGE_MOJOM_APP_TYPE_MOJOM_TRAITS_H_

#include "chrome/browser/ui/webui/settings/ash/os_apps_page/mojom/app_notification_handler.mojom.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

namespace {

using Readiness = chromeos::settings::app_notification::mojom::Readiness;
using PermissionDataView =
    chromeos::settings::app_notification::mojom::PermissionDataView;
using PermissionType =
    chromeos::settings::app_notification::mojom::PermissionType;
using TriState = chromeos::settings::app_notification::mojom::TriState;
using PermissionValueDataView =
    chromeos::settings::app_notification::mojom::PermissionValueDataView;

}  // namespace

template <>
struct EnumTraits<Readiness, apps::Readiness> {
  static Readiness ToMojom(apps::Readiness input);
  static bool FromMojom(Readiness input, apps::Readiness* output);
};

template <>
struct StructTraits<PermissionDataView, apps::PermissionPtr> {
  static apps::PermissionType permission_type(const apps::PermissionPtr& r) {
    return r->permission_type;
  }

  static const apps::PermissionValuePtr& value(const apps::PermissionPtr& r) {
    return r->value;
  }

  static bool is_managed(const apps::PermissionPtr& r) { return r->is_managed; }

  static bool Read(PermissionDataView, apps::PermissionPtr* out);
};

template <>
struct CloneTraits<apps::PermissionPtr> {
  static apps::PermissionPtr Clone(const apps::PermissionPtr& input) {
    return input->Clone();
  }
};

template <>
struct EnumTraits<PermissionType, apps::PermissionType> {
  static PermissionType ToMojom(apps::PermissionType input);
  static bool FromMojom(PermissionType input, apps::PermissionType* output);
};

template <>
struct EnumTraits<TriState, apps::TriState> {
  static TriState ToMojom(apps::TriState input);
  static bool FromMojom(TriState input, apps::TriState* output);
};

template <>
struct UnionTraits<PermissionValueDataView, apps::PermissionValuePtr> {
  static PermissionValueDataView::Tag GetTag(const apps::PermissionValuePtr& r);

  static bool IsNull(const apps::PermissionValuePtr& r) {
    return !r->bool_value.has_value() && !r->tristate_value.has_value();
  }

  static void SetToNull(apps::PermissionValuePtr* out) { out->reset(); }

  static bool bool_value(const apps::PermissionValuePtr& r) {
    return r->bool_value.value();
  }

  static apps::TriState tristate_value(const apps::PermissionValuePtr& r) {
    return r->tristate_value.value();
  }

  static bool Read(PermissionValueDataView data, apps::PermissionValuePtr* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_APPS_PAGE_MOJOM_APP_TYPE_MOJOM_TRAITS_H_
