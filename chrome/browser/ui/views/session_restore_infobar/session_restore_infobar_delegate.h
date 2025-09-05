// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"

namespace session_restore_infobar {

// An infobar delegate that informs the user that their session restore setting
// can be changed in settings.
class SessionRestoreInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Enum for the message type to be displayed in the infobar.
  enum class InfobarMessageType {
    kNone,
    // Infobar message displayed for turning off session restore from restart.
    kTurnOffFromRestart,
    // Infobar message displayed for turning off session restore from restored
    // session.
    kTurnOffFromSession,
    // Infobar message displayed for turning on session restore.
    kTurnOnSessionRestore,
  };

  explicit SessionRestoreInfoBarDelegate(base::OnceCallback<void()> close_cb,
                                         InfobarMessageType message_type);
  SessionRestoreInfoBarDelegate(const SessionRestoreInfoBarDelegate&) = delete;
  SessionRestoreInfoBarDelegate& operator=(
      const SessionRestoreInfoBarDelegate&) = delete;
  ~SessionRestoreInfoBarDelegate() override;

  // Creates a session restore infobar and adds it to the provided
  // `infobar_manager`. The `infobar_manager` will own the returned infobar.
  static void Show(content::WebContents* contents,
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
  void InfoBarDismissed() override;

 private:
  base::OnceCallback<void()> close_cb_;
  const InfobarMessageType message_type_;
};

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_DELEGATE_H_
