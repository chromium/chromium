// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/base/interaction/element_identifier.h"

namespace views {
class Label;
class MdTextButton;
}  // namespace views

// An infobar that shows a message, up to two optional buttons, and an optional,
// right-aligned link.  This is commonly used to do things like:
// "Would you like to do X?  [Yes]  [No]    _Learn More_ [x]"
class ConfirmInfoBar : public InfoBarView {
  METADATA_HEADER(ConfirmInfoBar, InfoBarView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOkButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonElementId);

  // Creates the appropriate ConfirmInfoBar subclass depending on whether the
  // delegate uses inline links.
  static std::unique_ptr<ConfirmInfoBar> Create(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);

  ConfirmInfoBar(const ConfirmInfoBar&) = delete;
  ConfirmInfoBar& operator=(const ConfirmInfoBar&) = delete;

  ~ConfirmInfoBar() override;

  // InfoBarView:
  void Layout(PassKey) override;

  ConfirmInfoBarDelegate* GetDelegate();
  const ConfirmInfoBarDelegate* GetDelegate() const;

  // label_for_testing() is implemented in the subclasses that use it.
  virtual views::Label* label_for_testing();
  views::MdTextButton* ok_button_for_testing() { return ok_button_; }
  views::MdTextButton* cancel_button_for_testing() { return cancel_button_; }

  int target_height_for_testing() const { return target_height(); }

 protected:
  explicit ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate);

  // Subclasses call this to set the label view and apply layout properties.
  // Adds the message label to the content container.
  template <typename T>
  T* AssignMessageLabel(std::unique_ptr<T> view);

  // InfoBarView:
  int GetContentMinimumWidth() const override;
  int GetContentPreferredWidth() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ConfirmInfoBarTest, StandardMessageUsesLabel);
  FRIEND_TEST_ALL_PREFIXES(ConfirmInfoBarTest, StandardMessageUsesEliding);

  void OkButtonPressed();
  void CancelButtonPressed();

  raw_ptr<views::MdTextButton> ok_button_ = nullptr;
  raw_ptr<views::MdTextButton> cancel_button_ = nullptr;
  raw_ptr<views::Link> link_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
