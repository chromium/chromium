// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/views/controls/link_listener.h"

class ElevationIconSetter;

namespace views {
class Button;
class Label;
class MdTextButton;
}

// An infobar that shows a message, up to two optional buttons, and an optional,
// right-aligned link.  This is commonly used to do things like:
// "Would you like to do X?  [Yes]  [No]               _Learn More_ [x]"
class ConfirmInfoBar : public InfoBarView,
                       public views::LinkListener {
 public:
  explicit ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate);
  ~ConfirmInfoBar() override;

  // InfoBarView:
  void Layout() override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

 protected:
  // InfoBarView:
  int ContentMinimumWidth() const override;

  ConfirmInfoBarDelegate* GetDelegate();

 private:
  // Creates a button suitable for use as either OK or Cancel.
  views::MdTextButton* CreateButton(ConfirmInfoBarDelegate::InfoBarButton type);

  // Returns the width of all content other than the label and link.  Layout()
  // uses this to determine how much space the label and link can take.
  int NonLabelWidth() const;

  views::Label* label_ = nullptr;
  views::MdTextButton* ok_button_ = nullptr;
  views::MdTextButton* cancel_button_ = nullptr;
  views::Link* link_ = nullptr;
  std::unique_ptr<ElevationIconSetter> elevation_icon_setter_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
