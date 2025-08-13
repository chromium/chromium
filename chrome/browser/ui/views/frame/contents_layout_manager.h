// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_LAYOUT_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_LAYOUT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"

// WARNING: DO NOT ADD NEW FEATURES HERE. New views in the content area
// should be added to ContentsContainerView. This is so that we can
// support Split View where there will be multiple instances of Contents
// WebView and associated overlay views. Please consult estalin@ and
// corising@ for any additional guidance.
class ContentsLayoutManager : public views::LayoutManagerBase {
 public:
  ContentsLayoutManager(views::View* contents_view,
                        views::View* lens_overlay_view);

  ContentsLayoutManager(const ContentsLayoutManager&) = delete;
  ContentsLayoutManager& operator=(const ContentsLayoutManager&) = delete;

  ~ContentsLayoutManager() override;

 protected:
  // views::LayoutManagerBase overrides:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  const raw_ptr<views::View> contents_view_;
  const raw_ptr<views::View> lens_overlay_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_LAYOUT_MANAGER_H_
