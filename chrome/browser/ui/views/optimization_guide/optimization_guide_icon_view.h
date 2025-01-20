// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_ICON_VIEW_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"

class Browser;
class OptimizationGuideKeyedService;

namespace optimization_guide::proto {
class OptimizationGuideIconViewMetadata;
}  // namespace optimization_guide::proto

// This icon appears in the location bar when the current page has a page
// entities optimization guide hint.
class OptimizationGuideIconView : public PageActionIconView {
  METADATA_HEADER(OptimizationGuideIconView, PageActionIconView)

 public:
  OptimizationGuideIconView(IconLabelBubbleView::Delegate* parent_delegate,
                            Delegate* delegate,
                            Browser* browser);
  ~OptimizationGuideIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;

 private:
  // Returns the metadata, if available, for the current web contents.
  std::optional<optimization_guide::proto::OptimizationGuideIconViewMetadata>
  GetMetadata() const;

  raw_ptr<OptimizationGuideKeyedService> optimization_guide_service_;

  base::WeakPtrFactory<OptimizationGuideIconView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_ICON_VIEW_H_
