// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRODUCT_SPECIFICATIONS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRODUCT_SPECIFICATIONS_ICON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"

class Browser;

// This icon appears in the location bar when the current page qualifies for
// showing product specifications.
class ProductSpecificationsIconView : public PageActionIconView {
  METADATA_HEADER(ProductSpecificationsIconView, PageActionIconView)

 public:
  ProductSpecificationsIconView(IconLabelBubbleView::Delegate* parent_delegate,
                                Delegate* delegate,
                                Browser* browser);
  ~ProductSpecificationsIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;

  void ForceVisibleForTesting(bool is_added);

 protected:
  // PageActionIconView:
  const gfx::VectorIcon& GetVectorIcon() const override;
  void UpdateImpl() override;

 private:
  // IconLabelBubbleView:
  void AnimationProgressed(const gfx::Animation* animation) override;

  bool ShouldShow();
  void SetVisualState(bool is_added);
  void MaybeShowPageActionLabel();
  void HidePageActionLabel();
  bool IsInProductSpecificationsSet() const;
  void ShowConfirmationToast(std::u16string set_name);

  const raw_ptr<Browser> browser_;

  raw_ptr<const gfx::VectorIcon> icon_;

  // Boolean that tracks whether we should extend the duration for which the
  // label is shown when it animates in.
  bool should_extend_label_shown_duration_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRODUCT_SPECIFICATIONS_ICON_VIEW_H_
