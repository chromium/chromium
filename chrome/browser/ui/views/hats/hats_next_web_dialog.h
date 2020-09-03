// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HATS_HATS_NEXT_WEB_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_HATS_HATS_NEXT_WEB_DIALOG_H_

#include "chrome/browser/profiles/profile_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Browser;
class Profile;

namespace views {
class Widget;
}  // namespace views

// A dialog for displaying a Happiness Tracking Survey (HaTS) NEXT survey to
// the user. The dialog presents a WebContents which connects to a publicly
// accessible, Chrome specific, webpage which is responsible for displaying the
// survey to users. The webpage has additional logic to provide information to
// this dialog via URL fragments, such as whether a survey is ready to be shown
// to the user.
class HatsNextWebDialog : public ui::WebDialogDelegate,
                          public views::BubbleDialogDelegateView,
                          public content::WebContentsDelegate,
                          public ProfileObserver {
 public:
  HatsNextWebDialog(Browser* browser, const std::string& trigger_id);
  ~HatsNextWebDialog() override;
  HatsNextWebDialog(const HatsNextWebDialog&) = delete;
  HatsNextWebDialog& operator=(const HatsNextWebDialog&) = delete;

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  ui::WebDialogDelegate::FrameKind GetWebDialogFrameKind() const override;

  // BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 protected:
  friend class MockHatsNextWebDialog;
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest, SurveyLoaded);

  HatsNextWebDialog(Browser* browser,
                    const std::string& trigger_id,
                    const GURL& hats_survey_url_,
                    const base::TimeDelta& timeout);

  class WebContentsDelegate;
  class WebContentsObserver;

  // Closes the HaTS Next widget and informs the service that the dialog was
  // shut.
  void CloseWidgetAndInformService();

  // Fired by the observer when the survey page has pushed state to the window
  // via URL fragments.
  void OnSurveyStateUpdateReceived(std::string state);

  // Provides mechanism to override URL requested by the dialog. Must be called
  // before CreateWebDialog() to take effect.
  void SetHatsSurveyURLforTesting(GURL url);

  // Displays the widget to the user, called when the dialog believes a survey
  // ready for display. Virtual to allow mocking in tests.
  virtual void ShowWidget();

  // Called by the dialog to close the widget due to timeout or the survey being
  // closed. Virtual to allow mocking in tests.
  virtual void CloseWidget();

  // Updates dialog size, provided via state update by the webpage. Virtual to
  // allow mocking in tests.
  virtual void UpdateWidgetSize(gfx::Size size);

  // Returns whether the dialog is still waiting for the survey to load.
  bool IsWaitingForSurveyForTesting();

 private:
  // A timer to prevent unresponsive loading of survey dialog.
  base::OneShotTimer loading_timer_;

  // The off-the-record profile used for browsing to the Chrome HaTS webpage.
  Profile* otr_profile_;

  Browser* browser_;

  // The HaTS Next survey trigger ID that is provided to the HaTS webpage.
  const std::string trigger_id_;

  // The size of the dialog. Desired dimensions are provided by the site loaded
  // in the web contents. Initialised to arbitrary non-zero value as creation
  // of 0 sized windows is disallowed on OSX.
  gfx::Size size_ = gfx::Size(10, 10);

  views::WebDialogView* web_view_ = nullptr;
  views::Widget* widget_ = nullptr;

  std::unique_ptr<WebContentsDelegate> web_contents_delegate_;
  std::unique_ptr<WebContentsObserver> web_contents_observer_;
  GURL hats_survey_url_;

  base::TimeDelta timeout_;

  base::WeakPtrFactory<HatsNextWebDialog> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_HATS_HATS_NEXT_WEB_DIALOG_H_
