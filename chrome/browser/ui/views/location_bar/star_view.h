// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/prefs/pref_member.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;
class CommandUpdater;

// The star icon to show a bookmark bubble.
class StarView : public PageActionIconView {
  METADATA_HEADER(StarView, PageActionIconView)

 public:
  StarView(CommandUpdater* command_updater,
           Browser* browser,
           IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
           PageActionIconView::Delegate* page_action_icon_delegate);
  StarView(const StarView&) = delete;
  StarView& operator=(const StarView&) = delete;
  ~StarView() override;

  // ui::PropertyHandler:
  void AfterPropertyChange(const void* key, int64_t old_value) override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

 private:
  void EditBookmarksPrefUpdated();

  BooleanPrefMember edit_bookmarks_enabled_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
