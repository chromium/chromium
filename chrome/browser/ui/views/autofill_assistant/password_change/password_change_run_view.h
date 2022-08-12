// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class View;
}  // namespace views

class PasswordChangeRunController;
class PasswordChangeRunProgress;

// View that displays the automatic password change experience. When this class
// is instantiated its ownership is given to the `display_delegate` passed
// during construction.
class PasswordChangeRunView : public views::View,
                              public PasswordChangeRunDisplay {
 public:
  METADATA_HEADER(PasswordChangeRunView);

  // IDs that identify a view within the dialog that was used in browsertests.
  enum class ChildrenViewsIds : int {
    none = 0,

    kTitleContainer,
    kTitle,
    kSuggestedPassword,

    kBody,
    kLineSeparator,
    kDescription,
    kButtonContainer,
  };

  PasswordChangeRunView(base::WeakPtr<PasswordChangeRunController> controller,
                        raw_ptr<AssistantDisplayDelegate> display_delegate);
  ~PasswordChangeRunView() override;

  PasswordChangeRunView(const PasswordChangeRunView&) = delete;
  PasswordChangeRunView& operator=(const PasswordChangeRunView&) = delete;

  // PasswordChangeRunDisplay:
  void Show() override;
  void SetTopIcon(
      autofill_assistant::password_change::TopIcon top_icon) override;
  void SetTitle(const std::u16string& title) override;
  void SetDescription(const std::u16string& progress_description) override;
  void SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep progress_step) override;
  autofill_assistant::password_change::ProgressStep GetProgressStep() override;
  void ShowBasePrompt(const std::u16string& description,
                      const std::vector<PromptChoice>& options) override;
  void ShowBasePrompt(const std::vector<PromptChoice>& options) override;
  void ShowUseGeneratedPasswordPrompt(
      const std::u16string& title,
      const std::u16string& generated_password,
      const std::u16string& description,
      const PromptChoice& manual_password_choice,
      const PromptChoice& generated_password_choice) override;
  void ClearPrompt() override;
  void ShowStartingScreen(const GURL& url) override;
  void ShowCompletionScreen(
      base::RepeatingClosure done_button_callback) override;
  void ShowErrorScreen() override;
  void OnControllerGone() override;

  // Returns a weak pointer to itself.
  base::WeakPtr<PasswordChangeRunView> GetWeakPtr();

 private:
  // Creates/initialises the view.
  void CreateView();

  // Renders the options for a base prompt.
  void CreateBasePromptOptions(const std::vector<PromptChoice>& choices);

  // Closes the view by removing itself from the display.
  // This method destroys an instance of this class.
  void Close();

  // Method that updates the UI to render the completion screen. This is called
  // only AFTER `password_change_run_progress_` is completed, both in terms of
  // steps and animation. Runs `show_completion_screen_done_button_callback_`
  // when user clicks on Done.
  void OnShowCompletionScreen();

  // The controller belonging to this view.
  base::WeakPtr<PasswordChangeRunController> controller_;

  // The display that owns this view.
  raw_ptr<AssistantDisplayDelegate> display_delegate_;

  raw_ptr<views::ImageView> top_icon_ = nullptr;
  raw_ptr<views::View> title_container_ = nullptr;
  raw_ptr<PasswordChangeRunProgress> password_change_run_progress_ = nullptr;

  // The body is used to render content below the title, i.e
  // prompts and descriptions.
  raw_ptr<views::View> body_ = nullptr;

  // Callback run when a user clicks Done after a successful run.
  base::RepeatingClosure show_completion_screen_done_button_callback_;
  // Factory for weak pointers to this view.
  base::WeakPtrFactory<PasswordChangeRunView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_VIEW_H_
