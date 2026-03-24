// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_CONVERTERS_TAB_CONVERTERS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_CONVERTERS_TAB_CONVERTERS_H_

#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_api/types/tab_states.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_network_state.h"
#include "ui/color/color_provider.h"

namespace tabs_api::converters {

tabs_api::mojom::TabPtr BuildMojoTab(tabs::TabInterface* tab,
                                     const ui::ColorProvider& color_provider,
                                     const types::TabStates& states);

// Builds a mojom::DataPtr based off a TabCollection.
// Note: Handle must be valid and point to a live TabCollection. There is a
// CHECK to enforce that precondition.
tabs_api::mojom::DataPtr BuildMojoTabCollectionData(
    tabs::TabCollectionHandle handle);

// Builds a field mask based on the internal TabChangeType.
tabs_api::mojom::TabFieldMaskPtr BuildTabFieldMask(TabChangeType type);

// Builds a field mask specifically for selection or activation changes.
tabs_api::mojom::TabFieldMaskPtr BuildTabFieldMaskForSelection(bool active,
                                                               bool selected);

}  // namespace tabs_api::converters

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_CONVERTERS_TAB_CONVERTERS_H_
