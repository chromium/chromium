// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_NO_SINKS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_NO_SINKS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

class Profile;

namespace media_router {

// This is the view that is shown in Cast dialog when no sinks have been
// discovered.
// - If the browser doesn't have the system permission for local discovery, an
// error message is shown, prompting users to enable the permission to use the
// feature.
// - If no sinks have been discovered, for three seconds after
// instantiation it shows a throbber, and after that it shows an icon that links
// to a help center article.
class CastDialogNoSinksView : public views::View {
  METADATA_HEADER(CastDialogNoSinksView, views::View)

 public:
  static constexpr base::TimeDelta kSearchWaitTime = base::Seconds(3);

  // `permission_rejected`: True if the view is shown because the system
  // permission to discover sinks has been rejected.
  CastDialogNoSinksView(Profile* profile, bool permission_rejected);
  CastDialogNoSinksView(const CastDialogNoSinksView&) = delete;
  CastDialogNoSinksView& operator=(const CastDialogNoSinksView) = delete;
  ~CastDialogNoSinksView() override;

  const base::OneShotTimer& timer_for_testing() const { return timer_; }
  const views::View* icon_for_testing() const { return icon_; }
  const std::u16string& label_text_for_testing() const {
    return label_->GetText();
  }
  const std::u16string& permission_rejected_label_text_for_testing() const {
    return permission_rejected_label_->GetText();
  }

 private:
  void SetHelpIcon();
  void ShowNoDeviceFoundView();

  const raw_ptr<Profile> profile_;
  base::OneShotTimer timer_;
  raw_ptr<views::View, DanglingUntriaged> icon_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::StyledLabel> permission_rejected_label_ = nullptr;
  bool permission_rejected_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_NO_SINKS_VIEW_H_
