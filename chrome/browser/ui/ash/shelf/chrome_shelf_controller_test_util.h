// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_CONTROLLER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_CONTROLLER_TEST_UTIL_H_

#include "ash/public/cpp/shelf_types.h"
#include "ui/events/types/event_type.h"

class GURL;

// Calls ShelfItemDelegate::ItemSelected for the item with the given |id|, using
// an event corresponding to the requested |event_type| and plumbs the requested
// |display_id| (invalid display id is mapped the primary display).
ash::ShelfAction SelectShelfItem(
    const ash::ShelfID& id,
    ui::EventType event_type,
    int64_t display_id,
    ash::ShelfLaunchSource source = ash::LAUNCH_FROM_UNKNOWN);

// Sets the refocus url for the item with the given |id|. The item must already
// exist in the shelf, and the delegate must be an instance of
// AppShortcutShelfItemController.
void SetRefocusURL(const ash::ShelfID& id, const GURL& url);

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_CONTROLLER_TEST_UTIL_H_
