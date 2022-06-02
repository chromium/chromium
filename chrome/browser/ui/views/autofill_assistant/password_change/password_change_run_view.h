// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

class PasswordChangeRunController;

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
    kDescription
  };

  PasswordChangeRunView(base::WeakPtr<PasswordChangeRunController> controller,
                        raw_ptr<AssistantDisplayDelegate> display_delegate);
  ~PasswordChangeRunView() override;

  PasswordChangeRunView(const PasswordChangeRunView&) = delete;
  PasswordChangeRunView& operator=(const PasswordChangeRunView&) = delete;

  // PasswordChangeRunDisplay
  void Show() override;
  void SetTopIcon(
      autofill_assistant::password_change::TopIcon top_icon) override;
  void SetTitle(const std::u16string& title) override;
  void SetDescription(const std::u16string& progress_description) override;
  void SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep progress_step) override;
  void ShowBasePrompt(const std::vector<std::string>& options) override;
  void ShowSuggestedPasswordPrompt(
      const std::u16string& suggested_password) override;
  void OnControllerGone() override;

  // Returns a weak pointer to itself.
  base::WeakPtr<PasswordChangeRunView> GetWeakPtr();

 private:
  // Create/initialise the view.
  void CreateView();

  // Closes the view by removing itself from the display.
  // This method destroys an instance of this class.
  void Close();

  // The controller belonging to this view.
  base::WeakPtr<PasswordChangeRunController> controller_;
  // The display that owns this view.
  raw_ptr<AssistantDisplayDelegate> display_delegate_;

  raw_ptr<views::ImageView> top_icon_ = nullptr;
  raw_ptr<views::View> title_container_ = nullptr;

  // The body is used to render stuff under the title, i.e
  // prompts and descriptions.
  raw_ptr<views::View> body_ = nullptr;

  // Factory for weak pointers to this view.
  base::WeakPtrFactory<PasswordChangeRunView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_VIEW_H_
