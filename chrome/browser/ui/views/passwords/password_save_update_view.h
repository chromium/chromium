// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/token.h"
#include "chrome/browser/ui/passwords/bubble_controllers/save_update_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/view.h"

namespace views {
class EditableCombobox;
class EditablePasswordCombobox;
}  // namespace views

namespace ui {
class SimpleMenuModel;
}

class PasswordSaveUpdateExperimentButtonRow;

// A view offering the user the ability to save or update credentials (depending
// on |is_update_bubble|) either in the profile and/or account stores. Contains
// a username and password field. In addition, it contains a "Save"/"Update"
// button and a "Never"/"Nope" button.
class PasswordSaveUpdateView : public PasswordBubbleViewBase,
                               public views::WidgetObserver {
  METADATA_HEADER(PasswordSaveUpdateView, PasswordBubbleViewBase)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPasswordBubbleElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kExtraButtonElementId);

  enum ViewIds {
    kSplitButton = 1,
    kDismissUpdateButton,
    kOkButton,
    kCaretButton,
    kNotNowButton,
    kCustomButtonRow
  };

  PasswordSaveUpdateView(content::WebContents* web_contents,
                         views::BubbleAnchor anchor_view,
                         DisplayReason reason);

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

#ifdef UNIT_TEST
  views::EditableCombobox* username_dropdown_for_testing() const {
    return username_dropdown_.get();
  }

  views::EditablePasswordCombobox* password_dropdown_for_testing() const {
    return password_dropdown_.get();
  }

  views::MdTextButton* extra_view_for_testing() const {
    return extra_view_.get();
  }

  void TriggerOnContentChangedForTesting() { OnContentChanged(); }
#endif  // #ifdef UNIT_TEST

  ui::SimpleMenuModel* MenuModelForTesting() const;
  views::MdTextButton* GetOkButtonForTesting() const;
  views::MdTextButton* GetCancelButtonForTesting() const;

 private:
  ~PasswordSaveUpdateView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // If the sign in promo should be shown, it will remove the current contents
  // of the bubble and replace them with the sign in promo. Returns true if the
  // bubble is not replaced with the promo, and therefore closed. Returns false
  // if it is replaced, which will cause the bubble to stay open.
  bool CloseOrReplaceWithPromo();

  // PasswordBubbleViewBase:
  views::View* GetInitiallyFocusedView() override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  ui::ImageModel GetWindowIcon() override;

  // View:
  void AddedToWidget() override;

  bool IsSaveBubbleDropdownExperimentEnabled() const;
  void UpdateUsernameAndPasswordInModel();
  void UpdateBubbleUIElements();
  std::unique_ptr<views::View> CreateFooterView();

  // Announces to the screen readers a change in the bubble between Save and
  // Update states, or the Sign-in promo.
  void AnnounceBubbleChange();

  // Used for both the username and password editable comboboxes.
  void OnContentChanged();

  // Should be called only after the bubble has been displayed.
  void UpdateFootnote();

  // Invoked when the user clicks on the eye image button in the password
  // dropdown. It invokes the controller to determine if the user should be able
  // to unmask the password.
  void TogglePasswordRevealed();

  SaveUpdateBubbleController controller_;

  // True iff it is an update password bubble on creation. False iff it is a
  // save bubble.
  const bool is_update_bubble_;

  // The views for the username and password dropdown elements.
  raw_ptr<views::EditableCombobox> username_dropdown_ = nullptr;
  raw_ptr<views::EditablePasswordCombobox> password_dropdown_ = nullptr;

  // Hidden view that will contain status text for immediate output by
  // screen readers when the bubble changes state between Save and Update.
  raw_ptr<views::View> accessibility_alert_ = nullptr;

  // Points to the "not now" button when present.
  raw_ptr<views::MdTextButton> extra_view_ = nullptr;

  // The custom button row container occupying the bottom area when
  // `kPasswordSaveUpdateDropdownMenuExperiment` is enabled.
  raw_ptr<PasswordSaveUpdateExperimentButtonRow> custom_button_row_ = nullptr;

  std::unique_ptr<CloseOnDeactivatePin> reveal_password_pin_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_
