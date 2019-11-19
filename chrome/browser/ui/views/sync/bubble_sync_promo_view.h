// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_BUBBLE_SYNC_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_BUBBLE_SYNC_PROMO_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"

class BubbleSyncPromoDelegate;

// Bookmark sync promo displayed at the bottom of the bookmark bubble.
class BubbleSyncPromoView : public views::StyledLabel,
                            public views::StyledLabelListener {
 public:
  // |delegate| is not owned by BubbleSyncPromoView.
  BubbleSyncPromoView(BubbleSyncPromoDelegate* delegate,
                      signin_metrics::AccessPoint access_point,
                      int link_text_resource_id,
                      int message_text_resource_id);
  ~BubbleSyncPromoView() override;

 private:
  // views::StyledLabel:
  const char* GetClassName() const override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // Delegate, to handle clicks on the sign in link.
  BubbleSyncPromoDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BubbleSyncPromoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_BUBBLE_SYNC_PROMO_VIEW_H_
