// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_AI_MODE_PAGE_ACTION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_AI_MODE_PAGE_ACTION_ICON_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserWindowInterface;
class PrefChangeRegistrar;

namespace gfx {
struct VectorIcon;
}

namespace ui {
class KeyEvent;
}

namespace views {
class BubbleDialogDelegate;
}

class AiModePageActionIconView : public PageActionIconView {
  METADATA_HEADER(AiModePageActionIconView, PageActionIconView)

 public:
  AiModePageActionIconView(IconLabelBubbleView::Delegate* parent_delegate,
                           Delegate* delegate,
                           BrowserWindowInterface* browser);
  AiModePageActionIconView(const AiModePageActionIconView&) = delete;
  AiModePageActionIconView& operator=(const AiModePageActionIconView&) = delete;
  ~AiModePageActionIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  void ExecuteWithKeyboardSourceForTesting();

 protected:
  // PageActionIconView:
  void UpdateImpl() override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;

  std::unique_ptr<PrefChangeRegistrar> pref_registrar_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_AI_MODE_PAGE_ACTION_ICON_VIEW_H_
