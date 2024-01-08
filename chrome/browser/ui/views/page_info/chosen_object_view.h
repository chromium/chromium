// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_CHOSEN_OBJECT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_CHOSEN_OBJECT_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/page_info/page_info_ui.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
}

class ChosenObjectViewObserver;
class RichControlsContainerView;

// A ChosenObjectView is a row in the Page Info bubble that shows an individual
// object (e.g. a Bluetooth device, a USB device) that the current site has
// access to.
class ChosenObjectView : public views::View {
  METADATA_HEADER(ChosenObjectView, views::View)

 public:
  explicit ChosenObjectView(std::unique_ptr<PageInfoUI::ChosenObjectInfo> info,
                            std::u16string display_name);
  ChosenObjectView(const ChosenObjectView&) = delete;
  ChosenObjectView& operator=(const ChosenObjectView&) = delete;
  ~ChosenObjectView() override;

  void AddObserver(ChosenObjectViewObserver* observer);
  void ResetPermission();

  // views::View:
  void OnThemeChanged() override;

 private:
  void UpdateIconImage(bool is_deleted) const;

  void ExecuteDeleteCommand();

  raw_ptr<views::ImageButton> delete_button_ = nullptr;
  raw_ptr<RichControlsContainerView> row_view_ = nullptr;

  base::ObserverList<ChosenObjectViewObserver>::Unchecked observer_list_;
  std::unique_ptr<PageInfoUI::ChosenObjectInfo> info_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_CHOSEN_OBJECT_VIEW_H_
