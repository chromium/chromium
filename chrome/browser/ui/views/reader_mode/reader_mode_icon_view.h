// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_READER_MODE_READER_MODE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_READER_MODE_READER_MODE_ICON_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;
class PrefService;

// A location bar icon that toggles Reader Mode for the current page.
class ReaderModeIconView : public PageActionIconView,
                           public dom_distiller::DistillabilityObserver,
                           public content::WebContentsObserver {
 public:
  METADATA_HEADER(ReaderModeIconView);
  ReaderModeIconView(CommandUpdater* command_updater,
                     IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                     PageActionIconView::Delegate* page_action_icon_delegate,
                     PrefService* pref_service);
  ReaderModeIconView(const ReaderModeIconView&) = delete;
  ReaderModeIconView& operator=(const ReaderModeIconView&) = delete;
  ~ReaderModeIconView() override;

 protected:
  // content:WebContentsObserver overrides:
  // Detect when navigation to the distilled page completes. This is required to
  // correctly update the icon's inkdrop.
  void PrimaryPageChanged(content::Page& page) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;

  // PageActionIconView overrides:
  void UpdateImpl() override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;

  // GetBubble() is required by PageActionIconView; however, the icon
  // intentionally does not display a bubble when activated.
  views::BubbleDialogDelegate* GetBubble() const override;

  // dom_distiller::DistillabilityObserver overrides:
  void OnResult(const dom_distiller::DistillabilityResult& result) override;

 private:
  raw_ptr<PrefService> pref_service_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_READER_MODE_READER_MODE_ICON_VIEW_H_
