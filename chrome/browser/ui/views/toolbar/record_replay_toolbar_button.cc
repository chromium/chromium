// Copyright 2023 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/record_replay_toolbar_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "ui/views/controls/button/button_controller.h"

RecordReplayToolbarButton::RecordReplayToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&RecordReplayToolbarButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser) {
  SetVectorIcons(kRecordReplayIcon, kRecordReplayIcon);
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
}

RecordReplayToolbarButton::~RecordReplayToolbarButton() = default;

void RecordReplayToolbarButton::ButtonPressed() {
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  content::WebContents* old_web_contents = tab_strip_model->GetActiveWebContents();
  if (!old_web_contents) {
    return;
  }
  content::BrowserContext* browser_context = old_web_contents->GetBrowserContext();

  int index = tab_strip_model->GetIndexOfWebContents(old_web_contents);
  GURL url = old_web_contents->GetVisibleURL();

  // Create a new web-contents to load.
  content::WebContents::CreateParams new_params(browser_context);
  new_params.record_replay_for_recording = true;
  std::unique_ptr<content::WebContents> new_web_contents(
    content::WebContents::Create(new_params));
  content::WebContents* new_web_contents_ptr = new_web_contents.get();

  tab_strip_model->AppendWebContents(std::move(new_web_contents), true);

  // Re-get the index of the old web contents just in case it changed due to the
  // insert (it shouldn't have).)
  index = tab_strip_model->GetIndexOfWebContents(old_web_contents);

  // [RecordReplay] NOTE: This close-type doesn't remember history.  Do we want to?
  tab_strip_model->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);

  new_web_contents_ptr->GetController().LoadURL(url, content::Referrer(),
                                                ui::PAGE_TRANSITION_TYPED,
                                                std::string());
}
