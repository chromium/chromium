// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_TAB_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// An observer of WebContents that facilitates the logic for the customize
// chrome side panel. This per-tab class also owns the
// CustomizeChromeSidePanelController.
class CustomizeChromeTabHelper
    : public content::WebContentsUserData<CustomizeChromeTabHelper> {
 public:
  // This class delegates the responsibility for registering and deregistering
  // the Customize Chrome side panel.
  class Delegate {
   public:
    virtual void CreateAndRegisterEntry() = 0;
    virtual void DeregisterEntry() = 0;
    virtual void ShowCustomizeChromeSidePanel() = 0;
    virtual bool IsCustomizeChromeEntryShowing() const = 0;
    virtual bool IsCustomizeChromeEntryAvailable() const = 0;
    virtual ~Delegate() = default;
  };

  using StateChangedCallBack = base::RepeatingCallback<void(bool)>;

  CustomizeChromeTabHelper(const CustomizeChromeTabHelper&) = delete;
  CustomizeChromeTabHelper& operator=(const CustomizeChromeTabHelper&) = delete;
  ~CustomizeChromeTabHelper() override;

  // Creates a WebUISidePanelView for customize chrome and registers
  // the customize chrome side panel entry.
  void CreateAndRegisterEntry();

  // Deregisters the customize chrome side panel entry.
  void DeregisterEntry();

  // Opens Side Panel to the customize chrome entry.
  void ShowCustomizeChromeSidePanel();

  // True if the side panel is open and showing the customize chrome entry.
  bool IsCustomizeChromeEntryShowing() const;

  // True if the customize chrome entry is available in current tabs' registry.
  bool IsCustomizeChromeEntryAvailable() const;

  // Called by when the side panel is shown/hidden, runs callback that
  // shows/hides the customize chrome button
  void EntryStateChanged(bool is_open);

  // Sets callback that is run when side panel entry state is changed
  void SetCallback(StateChangedCallBack callback);

 private:
  friend class content::WebContentsUserData<CustomizeChromeTabHelper>;
  explicit CustomizeChromeTabHelper(content::WebContents* web_contents);

  std::unique_ptr<Delegate> delegate_;

  StateChangedCallBack entry_state_changed_callback_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_TAB_HELPER_H_
