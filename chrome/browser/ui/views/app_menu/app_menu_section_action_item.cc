// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"

#include "ui/base/metadata/metadata_impl_macros.h"

AppMenuSectionActionItem::AppMenuSectionActionItem(const std::u16string& text) {
  SetText(text);
}

AppMenuSectionActionItem::~AppMenuSectionActionItem() = default;

BEGIN_METADATA(AppMenuSectionActionItem)
END_METADATA
