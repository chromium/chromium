// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/browser_app_instance_registry_mojom_traits.h"

namespace mojo {

bool StructTraits<crosapi::mojom::BrowserWindowInstanceUpdateDataView,
                  apps::BrowserWindowInstanceUpdate>::
    Read(crosapi::mojom::BrowserWindowInstanceUpdateDataView input,
         apps::BrowserWindowInstanceUpdate* output) {
  apps::BrowserWindowInstanceUpdate update;
  if (input.ReadId(&update.id) && input.ReadWindowId(&update.window_id)) {
    update.is_active = input.is_active();
    update.browser_session_id = input.browser_session_id();
    update.restored_browser_session_id = input.restored_browser_session_id();
    update.is_incognito = input.is_incognito();
    update.lacros_profile_id = input.lacros_profile_id();
    *output = std::move(update);
    return true;
  }
  return false;
}

bool StructTraits<crosapi::mojom::BrowserAppInstanceUpdateDataView,
                  apps::BrowserAppInstanceUpdate>::
    Read(crosapi::mojom::BrowserAppInstanceUpdateDataView input,
         apps::BrowserAppInstanceUpdate* output) {
  apps::BrowserAppInstanceUpdate update;
  if (input.ReadId(&update.id) && input.ReadType(&update.type) &&
      input.ReadAppId(&update.app_id) &&
      input.ReadWindowId(&update.window_id) && input.ReadTitle(&update.title)) {
    update.is_browser_active = input.is_browser_active();
    update.is_web_contents_active = input.is_web_contents_active();
    update.browser_session_id = input.browser_session_id();
    update.restored_browser_session_id = input.restored_browser_session_id();
    *output = std::move(update);
    return true;
  }
  return false;
}

crosapi::mojom::BrowserAppInstanceType
EnumTraits<crosapi::mojom::BrowserAppInstanceType,
           apps::BrowserAppInstanceUpdate::Type>::
    ToMojom(apps::BrowserAppInstanceUpdate::Type input) {
  switch (input) {
    case apps::BrowserAppInstanceUpdate::Type::kAppTab:
      return crosapi::mojom::BrowserAppInstanceType::kAppTab;
    case apps::BrowserAppInstanceUpdate::Type::kAppWindow:
      return crosapi::mojom::BrowserAppInstanceType::kAppWindow;
  }
}

bool EnumTraits<crosapi::mojom::BrowserAppInstanceType,
                apps::BrowserAppInstanceUpdate::Type>::
    FromMojom(crosapi::mojom::BrowserAppInstanceType input,
              apps::BrowserAppInstanceUpdate::Type* output) {
  switch (input) {
    case crosapi::mojom::BrowserAppInstanceType::kAppTab:
      *output = apps::BrowserAppInstanceUpdate::Type::kAppTab;
      return true;
    case crosapi::mojom::BrowserAppInstanceType::kAppWindow:
      *output = apps::BrowserAppInstanceUpdate::Type::kAppWindow;
      return true;
  }
}

}  // namespace mojo
