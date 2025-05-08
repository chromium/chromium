// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_utils.h"

namespace tabs_api::converters {

tabs_api::mojom::TabPtr BuildMojoTab(tabs::TabHandle handle,
                                     const TabRendererData& data) {
  auto result = tabs_api::mojom::Tab::New();

  result->id = tabs_api::TabId(tabs_api::TabId::Type::kContent,
                               base::NumberToString(handle.raw_value()));
  result->title = base::UTF16ToUTF8(data.title);
  // TODO(crbug.com/414630734). Integrate the favicon_url after it is
  // typemapped.
  result->url = data.visible_url;
  result->network_state = data.network_state;
  if (handle.Get() != nullptr) {
    for (const auto alert_state :
         GetTabAlertStatesForContents(handle.Get()->GetContents())) {
      result->alert_states.push_back(alert_state);
    }
  }

  return result;
}

}  // namespace tabs_api::converters
