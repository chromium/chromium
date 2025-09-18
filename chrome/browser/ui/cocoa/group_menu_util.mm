// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/group_menu_util.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "components/tabs/public/tab_group.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

namespace chrome {

void UpdateGroupIndicatorForMenuItem(
    NSMenuItem* item,
    std::optional<tab_groups::TabGroupColorId> tab_group_color_id) {
  if (!tab_group_color_id) {
    item.attributedTitle = nil;
    return;
  }

  // Create the group indicator image.
  constexpr int kTabGroupIndicatorSize = 8;
  const auto& color_provider =
      [AppController.sharedController lastActiveColorProvider];
  const ui::ColorId color_id =
      GetTabGroupContextMenuColorId(tab_group_color_id.value());
  gfx::ImageSkia group_icon = gfx::CreateVectorIcon(
      kTabGroupIcon, kTabGroupIndicatorSize, color_provider.GetColor(color_id));
  NSImage* image = NSImageFromImageSkia(group_icon);

  // Create text attachment to hold the group indicator image.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] initWithData:nil
                                                                 ofType:nil];
  attachment.image = image;
  attachment.bounds =
      NSMakeRect(0, 0, kTabGroupIndicatorSize, kTabGroupIndicatorSize);

  // Add to the menu item by setting attributedTitle with the attachment
  NSMutableAttributedString* attrTitle = [[NSMutableAttributedString alloc]
      initWithString:[NSString stringWithFormat:@"%@ ", item.title]];
  [attrTitle
      appendAttributedString:[NSAttributedString
                                 attributedStringWithAttachment:attachment]];
  item.attributedTitle = attrTitle;

  // TODO(crbug.com/441750325): Add screen reader support for group indicator.
}

}  // namespace chrome
