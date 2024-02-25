// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_ALTERNATE_NAV_INFOBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_ALTERNATE_NAV_INFOBAR_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"

class AlternateNavInfoBarDelegate;

// An infobar that shows a string with an embedded link.
class AlternateNavInfoBarView : public InfoBarView {
 public:
  explicit AlternateNavInfoBarView(
      std::unique_ptr<AlternateNavInfoBarDelegate> delegate);

  AlternateNavInfoBarView(const AlternateNavInfoBarView&) = delete;
  AlternateNavInfoBarView& operator=(const AlternateNavInfoBarView&) = delete;

  ~AlternateNavInfoBarView() override;

 private:
  // Treating |labels| as pieces of one continuous string, elides to fit
  // |available_width| so as to guarantee that a trailing ellipsis is always
  // displayed when the string is elided and there is at least room to display
  // a lone ellipsis.
  //
  // NOTE: This may modify the text of any/all of the labels, so reset their
  // texts when the available width changes before calling this again.
  static void ElideLabels(Labels* labels, int available_width);

  // InfoBarView:
  void Layout(PassKey) override;
  int GetContentMinimumWidth() const override;

  AlternateNavInfoBarDelegate* GetDelegate();

  std::u16string label_1_text_;
  std::u16string link_text_;
  std::u16string label_2_text_;

  raw_ptr<views::Label> label_1_ = nullptr;
  raw_ptr<views::Link> link_ = nullptr;
  raw_ptr<views::Label> label_2_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_ALTERNATE_NAV_INFOBAR_VIEW_H_
