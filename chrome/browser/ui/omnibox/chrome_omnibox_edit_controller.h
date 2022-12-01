// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"

class Browser;
class CommandUpdater;
class Profile;

namespace content {
class WebContents;
}

// Chrome-specific extension of the OmniboxEditController base class.
class ChromeOmniboxEditController : public OmniboxEditController {
 public:
  ChromeOmniboxEditController(const ChromeOmniboxEditController&) = delete;
  ChromeOmniboxEditController& operator=(const ChromeOmniboxEditController&) =
      delete;

  // OmniboxEditController:
  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match,
      IDNA2008DeviationCharacter deviation_char_in_hostname) override;
  void OnInputInProgress(bool in_progress) override;

  // Returns the WebContents of the currently active tab.
  virtual content::WebContents* GetWebContents();

  // Called when the the controller should update itself without restoring any
  // tab state.
  virtual void UpdateWithoutTabRestore();

  CommandUpdater* command_updater() { return command_updater_; }
  const CommandUpdater* command_updater() const { return command_updater_; }

 protected:
  ChromeOmniboxEditController(Browser* browser,
                              Profile* profile,
                              CommandUpdater* command_updater);
  ~ChromeOmniboxEditController() override;

 private:
  const raw_ptr<Browser> browser_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<CommandUpdater, DanglingUntriaged> command_updater_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_CONTROLLER_H_
