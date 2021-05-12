// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_CHOSEN_OBJECT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_CHOSEN_OBJECT_VIEW_H_

#include <string>

#include "components/page_info/page_info_ui.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
}  // namespace views

class ChosenObjectViewObserver;

// A ChosenObjectView is a row in the Page Info bubble that shows an individual
// object (e.g. a Bluetooth device, a USB device) that the current site has
// access to.
class ChosenObjectView : public views::View {
 public:
  METADATA_HEADER(ChosenObjectView);
  explicit ChosenObjectView(std::unique_ptr<PageInfoUI::ChosenObjectInfo> info,
                            std::u16string display_name);
  ChosenObjectView(const ChosenObjectView&) = delete;
  ChosenObjectView& operator=(const ChosenObjectView&) = delete;
  ~ChosenObjectView() override;

  void AddObserver(ChosenObjectViewObserver* observer);

  // views:View:
  void OnThemeChanged() override;

 private:
  void UpdateIconImage(bool is_deleted) const;

  void ExecuteDeleteCommand();

  views::ImageView* icon_;             // Owned by the views hierarchy.
  views::ImageButton* delete_button_;  // Owned by the views hierarchy.

  base::ObserverList<ChosenObjectViewObserver>::Unchecked observer_list_;
  std::unique_ptr<PageInfoUI::ChosenObjectInfo> info_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_CHOSEN_OBJECT_VIEW_H_
