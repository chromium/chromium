// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_glic_container.h"

#include <base/logging.h>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

TabGlicContainer::TabGlicContainer(TabStripController* tab_strip_controller) {
#if BUILDFLAG(ENABLE_GLIC)
  if (GlicEnabling::IsEnabledByFlags()) {
    std::unique_ptr<glic::GlicButton> glic_button =
        std::make_unique<glic::GlicButton>(tab_strip_controller);
    glic_button->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kCenter);
    glic_button->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0, GetLayoutConstant(TAB_STRIP_PADDING)));

    glic_button_ = AddChildView(std::move(glic_button));
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

TabGlicContainer::~TabGlicContainer() = default;

BEGIN_METADATA(TabGlicContainer)
END_METADATA
