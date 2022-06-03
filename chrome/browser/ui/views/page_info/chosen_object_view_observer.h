// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_CHOSEN_OBJECT_VIEW_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_CHOSEN_OBJECT_VIEW_OBSERVER_H_

#include "components/page_info/page_info_ui.h"

class ChosenObjectViewObserver {
 public:
  // This method is called when permission for the object is revoked.
  virtual void OnChosenObjectDeleted(
      const PageInfoUI::ChosenObjectInfo& info) = 0;

 protected:
  virtual ~ChosenObjectViewObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_CHOSEN_OBJECT_VIEW_OBSERVER_H_
