// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

class TranslateBubbleController
    : public content::WebContentsUserData<TranslateBubbleController>,
      public PartialTranslateBubbleModel::Observer {
 public:
  ~TranslateBubbleController() override;
  TranslateBubbleController(const TranslateBubbleController&) = delete;
  TranslateBubbleController& operator=(const TranslateBubbleController&) =
      delete;

  static TranslateBubbleController* GetOrCreate(
      content::WebContents* web_contents);

  // Shows the Full Page Translate bubble. Returns the newly created bubble's
  // Widget or nullptr in cases when the bubble already exists or when the
  // bubble is not created.
  views::Widget* ShowTranslateBubble(
      views::View* anchor_view,
      views::Button* highlighted_button,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type,
      LocationBarBubbleDelegateView::DisplayReason reason);

  // Initiates the Partial Translate request, showing the bubble after a delay
  // dependent on the Partial Translate response.
  void StartPartialTranslate(views::View* anchor_view,
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

 protected:
  explicit TranslateBubbleController(content::WebContents* contents);

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

  friend class content::WebContentsUserData<TranslateBubbleController>;
  friend class TranslateBubbleControllerTest;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<TranslateBubbleController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_CONTROLLER_H_
