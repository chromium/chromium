// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_NO_SINKS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_NO_SINKS_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

class Profile;

namespace media_router {

// This is the view that is shown in Cast dialog when no sinks have been
// discovered. For three seconds after instantiation it shows a throbber, and
// after that it shows an icon that links to a help center article.
class CastDialogNoSinksView : public views::View {
 public:
  static constexpr base::TimeDelta kSearchWaitTime =
      base::TimeDelta::FromSeconds(3);

  explicit CastDialogNoSinksView(Profile* profile);
  ~CastDialogNoSinksView() override;

  const base::OneShotTimer& timer_for_testing() const { return timer_; }
  const views::View* icon_for_testing() const { return icon_; }
  const base::string16& label_text_for_testing() const {
    return label_->GetText();
  }

 private:
  void SetHelpIconView();

  Profile* const profile_;
  base::OneShotTimer timer_;
  views::View* icon_ = nullptr;
  views::Label* label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CastDialogNoSinksView);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_NO_SINKS_VIEW_H_
