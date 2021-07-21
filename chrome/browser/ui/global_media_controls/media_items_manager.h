// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEMS_MANAGER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEMS_MANAGER_H_

#include <string>

class MediaItemsManager {
 public:
  // The notification with the given id should be shown.
  virtual void ShowItem(const std::string& id) = 0;

  // The notification with the given id should be hidden.
  virtual void HideItem(const std::string& id) = 0;

 protected:
  virtual ~MediaItemsManager() = default;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEMS_MANAGER_H_
