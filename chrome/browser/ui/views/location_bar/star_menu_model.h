// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_MENU_MODEL_H_

#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

class StarMenuModel : public ui::SimpleMenuModel {
 public:
  StarMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                bool bookmarked,
                bool can_move_to_read_later,
                bool exists_as_unread_in_read_later);
  StarMenuModel(const StarMenuModel&) = delete;
  StarMenuModel& operator=(const StarMenuModel&) = delete;
  ~StarMenuModel() override;

  enum StarMenuCommand {
    CommandBookmark,
    CommandMoveToReadLater,
    CommandMarkAsRead
  };

 private:
  void Build(bool bookmarked,
             bool can_move_to_read_later,
             bool exists_as_unread_in_read_later);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_MENU_MODEL_H_
