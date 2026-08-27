// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"

class Profile;

namespace session_restore_infobar {

// An infobar delegate that informs the user that their session restore setting
// can be changed in settings.
class SessionRestoreInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit SessionRestoreInfoBarDelegate(Profile& profile,
                                         base::OnceCallback<void()> close_cb,
                                         InfobarMessageType message_type);
  SessionRestoreInfoBarDelegate(const SessionRestoreInfoBarDelegate&) = delete;
  SessionRestoreInfoBarDelegate& operator=(
      const SessionRestoreInfoBarDelegate&) = delete;
  ~SessionRestoreInfoBarDelegate() override;

  // Creates a session restore infobar and adds it to the provided
  // `infobar_manager`. The `infobar_manager` will own the returned infobar.
  static infobars::InfoBar* Show(
      infobars::ContentInfoBarManager* infobar_manager,
      Profile& profile,
      base::OnceCallback<void()> close_cb,
      InfobarMessageType message_type);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  std::u16string GetMessageText() const override;
  std::u16string GetLinkText() const override;
  int GetButtons() const override;
  bool ShouldShowLinkBeforeButton() const override;
  int GetLinkSpacingWhenPositionedBeforeButton() const override;
  void InfoBarDismissed() override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  std::optional<std::u16string> GetLinkAccessibleText() const override;

 private:
  void OnSessionRestorePrefChanged();

  const raw_ref<Profile> profile_;
  base::OnceCallback<void()> close_cb_;
  const InfobarMessageType message_type_;
  bool action_taken_ = false;

  // Used to track changes to the session restore preference.
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_DELEGATE_H_
