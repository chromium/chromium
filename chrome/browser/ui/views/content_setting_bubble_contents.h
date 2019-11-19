// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
#define CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link_listener.h"

namespace views {
class ImageButton;
class RadioButton;
class LabelButton;
}

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
                                     public views::ButtonListener,
                                     public views::LinkListener,
                                     public views::ComboboxListener,
                                     public ContentSettingBubbleModel::Owner {
 public:
  ContentSettingBubbleContents(
      std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model,
      content::WebContents* web_contents,
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow);
  ~ContentSettingBubbleContents() override;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void WindowClosing() override;

  // ContentSettingBubbleModel::Owner:
  void OnListItemAdded(
      const ContentSettingBubbleModel::ListItem& item) override;
  void OnListItemRemovedAt(int index) override;
  int GetSelectedRadioOption() override;

 protected:
  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  bool Accept() override;
  bool Close() override;
  void OnThemeChanged() override;

 private:
  class ListItemContainer;

  // Applies coloring to the learn more button.
  void StyleLearnMoreButton();

  // Create the extra view for this dialog, which contains any subset of: a
  // "learn more" button and a "manage" button.
  std::unique_ptr<View> CreateHelpAndManageView();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::ComboboxListener:
  void OnPerformAction(views::Combobox* combobox) override;

  // Provides data for this bubble.
  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model_;

  ListItemContainer* list_item_container_ = nullptr;

  typedef std::vector<views::RadioButton*> RadioGroup;
  RadioGroup radio_group_;
  views::Link* custom_link_ = nullptr;
  views::LabelButton* manage_button_ = nullptr;
  views::Checkbox* manage_checkbox_ = nullptr;
  views::ImageButton* learn_more_button_ = nullptr;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingBubbleContents);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
