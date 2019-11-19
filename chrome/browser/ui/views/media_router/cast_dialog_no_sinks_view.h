// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_NO_SINKS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_NO_SINKS_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class Profile;

namespace media_router {

// This is the view that is shown in Cast dialog when no sinks have been
// discovered. For three seconds after instantiation it shows a throbber, and
// after that it shows an icon that links to a help center article.
class CastDialogNoSinksView : public views::View, public views::ButtonListener {
 public:
  explicit CastDialogNoSinksView(Profile* profile);
  ~CastDialogNoSinksView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Called by tests.
  views::View* looking_for_sinks_view_for_test() {
    return looking_for_sinks_view_;
  }
  views::View* help_icon_view_for_test() { return help_icon_view_; }

 private:
  // Hides |looking_for_sinks_view_| and shows |help_icon_view_|.
  void ShowHelpIconView();

  // Opens the help center article for troubleshooting sinks not found in a
  // new tab.
  void ShowHelpCenterArticle();

  views::View* CreateLookingForSinksView();
  views::View* CreateHelpIconView();

  // View temporarily shown that indicates sink discovery is ongoing.
  views::View* looking_for_sinks_view_ = nullptr;

  // View indicating no sinks were found and containing an icon that links to
  // a help center article.
  views::View* help_icon_view_ = nullptr;

  Profile* const profile_;

  base::WeakPtrFactory<CastDialogNoSinksView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastDialogNoSinksView);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_NO_SINKS_VIEW_H_
