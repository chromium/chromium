// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_CONTROLLER_H_

#include "base/macros.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"

class CommandUpdater;

namespace content {
class WebContents;
}

// Chrome-specific extension of the OmniboxEditController base class.
class ChromeOmniboxEditController : public OmniboxEditController {
 public:
  // OmniboxEditController:
  void OnAutocompleteAccept(const GURL& destination_url,
                            TemplateURLRef::PostContent* post_content,
                            WindowOpenDisposition disposition,
                            ui::PageTransition transition,
                            AutocompleteMatchType::Type type,
                            base::TimeTicks match_selection_timestamp) override;
  void OnInputInProgress(bool in_progress) override;

  // Returns the WebContents of the currently active tab.
  virtual content::WebContents* GetWebContents();

  // Called when the the controller should update itself without restoring any
  // tab state.
  virtual void UpdateWithoutTabRestore();

  CommandUpdater* command_updater() { return command_updater_; }
  const CommandUpdater* command_updater() const { return command_updater_; }

 protected:
  explicit ChromeOmniboxEditController(CommandUpdater* command_updater);
  ~ChromeOmniboxEditController() override;

 private:
  CommandUpdater* const command_updater_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOmniboxEditController);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_CONTROLLER_H_
