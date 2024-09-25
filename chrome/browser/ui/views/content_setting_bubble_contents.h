// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
#define CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"

namespace views {
class Combobox;
class ImageButton;
class RadioButton;
class LabelButton;
}  // namespace views

// ContentSettingBubbleContents is used when the user turns on different kinds
// of content blocking (e.g. "block images").  When viewing a page with blocked
// content, icons appear in the omnibox corresponding to the content types that
// were blocked, and the user can click one to get a bubble hosting a few
// controls.  This class provides the content of that bubble.  In general,
// these bubbles typically have a title, a pair of radio buttons for toggling
// the blocking settings for the current site, a close button, and a button to
// get to a more comprehensive settings management dialog.  A few types have
// more or fewer controls than this.
class ContentSettingBubbleContents : public content::WebContentsObserver,
                                     public views::BubbleDialogDelegateView,
                                     public ContentSettingBubbleModel::Owner {
  METADATA_HEADER(ContentSettingBubbleContents, views::BubbleDialogDelegateView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMainElementId);
  ContentSettingBubbleContents(
      std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model,
      content::WebContents* web_contents,
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow);
  ContentSettingBubbleContents(const ContentSettingBubbleContents&) = delete;
  ContentSettingBubbleContents& operator=(const ContentSettingBubbleContents&) =
      delete;
  ~ContentSettingBubbleContents() override;

  // views::BubbleDialogDelegateView:
  void WindowClosing() override;

  // ContentSettingBubbleModel::Owner:
  void OnListItemAdded(
      const ContentSettingBubbleModel::ListItem& item) override;
  void OnListItemRemovedAt(int index) override;
  int GetSelectedRadioOption() override;

  void managed_button_clicked_for_test() {
    content_setting_bubble_model_->is_UMA_for_test = true;
    content_setting_bubble_model_->OnManageButtonClicked();
  }

  void learn_more_button_clicked_for_test() {
    content_setting_bubble_model_->is_UMA_for_test = true;
    content_setting_bubble_model_->OnLearnMoreClicked();
  }

  std::u16string get_message_for_test() const {
    return content_setting_bubble_model_->bubble_content().message;
  }

 protected:
  // views::WidgetDelegate:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void OnThemeChanged() override;

 private:
  class ListItemContainer;

  // Applies coloring to the learn more button.
  void StyleLearnMoreButton();

  // Create the extra view for this dialog, which contains any subset of: a
  // "learn more" button and a "manage" button.
  std::unique_ptr<View> CreateHelpAndManageView();

  void LinkClicked(int row, const ui::Event& event);
  void CustomLinkClicked();

  void OnPerformAction(views::Combobox* combobox);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Provides data for this bubble.
  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model_;

  raw_ptr<ListItemContainer, DanglingUntriaged> list_item_container_ = nullptr;

  typedef std::vector<raw_ptr<views::RadioButton, VectorExperimental>>
      RadioGroup;
  RadioGroup radio_group_;
  raw_ptr<views::LabelButton, DanglingUntriaged> manage_button_ = nullptr;
  raw_ptr<views::Checkbox, DanglingUntriaged> manage_checkbox_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> learn_more_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
