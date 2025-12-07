// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

// Controls both TranslateBubbleView and PartialTranslateBubbleView shown for
// a given browser. This controller ensures only one of the two are shown at
// a time, and is responsible for creating/hiding the bubbles.
class TranslateBubbleController : public PartialTranslateBubbleModel::Observer {
 public:
  DECLARE_USER_DATA(TranslateBubbleController);

  // `root_action_item` is used to retrieve the correct Translate ActionItem.
  TranslateBubbleController(BrowserWindowInterface* browser_window,
                            actions::ActionItem* root_action_item);
  ~TranslateBubbleController() override;
  TranslateBubbleController(const TranslateBubbleController&) = delete;
  TranslateBubbleController& operator=(const TranslateBubbleController&) =
      delete;

  static TranslateBubbleController* From(BrowserWindowInterface* window);

  // Shows the Full Page Translate bubble. Returns the newly created bubble's
  // Widget or nullptr in cases when the bubble already exists or when the
  // bubble is not created.
  views::Widget* ShowTranslateBubble(
      content::WebContents* web_contents,
      views::View* anchor_view,
      views::Button* highlighted_button,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type,
      LocationBarBubbleDelegateView::DisplayReason reason);

  // Initiates the Partial Translate request, showing the bubble after a delay
  // dependent on the Partial Translate response.
  void StartPartialTranslate(content::WebContents* web_contents,
                             views::View* anchor_view,
                             views::Button* highlighted_button,
                             const std::string& source_language,
                             const std::string& target_language,
                             const std::u16string& text_selection);

  // Closes the current Partial or Full Page Translate bubble, if either exists.
  // At most one of these bubbles should be non-null at any given time.
  void CloseBubble();

  // Returns the currently shown Full Page Translate bubble view. Returns
  // nullptr if the bubble is not currently shown.
  TranslateBubbleView* GetTranslateBubble() const;

  // Returns the currently shown Partial Translate bubble view. Returns nullptr
  // if the bubble is not currently shown.
  PartialTranslateBubbleView* GetPartialTranslateBubble() const;

  void SetTranslateBubbleModelFactory(
      base::RepeatingCallback<std::unique_ptr<TranslateBubbleModel>()>
          callback);
  void SetPartialTranslateBubbleModelFactory(
      base::RepeatingCallback<std::unique_ptr<PartialTranslateBubbleModel>()>
          callback);

  base::OnceClosure GetOnTranslateBubbleClosedCallback();
  base::OnceClosure GetOnPartialTranslateBubbleClosedCallback();

 private:
  // Weak references for the two possible Translate bubble views. These will be
  // nullptr if no bubble is currently shown. At most one of these pointers
  // should be non-null at any given time.
  raw_ptr<TranslateBubbleView> translate_bubble_view_ = nullptr;
  raw_ptr<PartialTranslateBubbleView> partial_translate_bubble_view_ = nullptr;

  // Factories used to construct models for the two different Translate bubbles.
  // If the factory is null, the standard implementations -
  // TranslateBubbleModelImpl and PartialTranslateBubbleModelImpl - will be
  // used.
  base::RepeatingCallback<std::unique_ptr<TranslateBubbleModel>()>
      model_factory_callback_;
  base::RepeatingCallback<std::unique_ptr<PartialTranslateBubbleModel>()>
      partial_model_factory_callback_;

  // Creates the Partial Translate bubble or updates the bubble if it already
  // exists.
  void CreatePartialTranslateBubble(
      content::WebContents* web_contents,
      views::View* anchor_view,
      views::Button* highlighted_button,
      PartialTranslateBubbleModel::ViewState view_state,
      const std::string& source_language,
      const std::string& target_language,
      const std::u16string& source_text,
      const std::u16string& target_text,
      translate::TranslateErrors error_type);

  // Handlers for when Translate bubbles are closed.
  void OnTranslateBubbleClosed();
  void OnPartialTranslateBubbleClosed();

  // Called when the initial wait for the Partial Translate response expires.
  // Shows the waitspin view.
  void OnPartialTranslateWaitExpired();

  // PartialTranslateBubbleModel::Observer impl.
  void OnPartialTranslateComplete() override;

  // Timer used for handling the delay before showing Partial Translate bubble.
  base::OneShotTimer partial_translate_timer_;

  friend class TranslateBubbleControllerTest;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // The action item associated with showing a Translate UI.
  // The bubbles use this to appropriately configure its "IsBubbleShowing"
  // property.
  const raw_ptr<actions::ActionItem> action_item_;

  ui::ScopedUnownedUserData<TranslateBubbleController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<TranslateBubbleController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_CONTROLLER_H_
