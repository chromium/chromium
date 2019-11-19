// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_INSTALLER_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_ui_delegate.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class ImageView;
class Label;
class Link;
class ProgressBar;
}  // namespace views

class Profile;

// The Crostini installer. Provides details about Crostini to the user and
// installs it if the user chooses to do so.
class CrostiniInstallerView : public views::DialogDelegateView,
                              public views::LinkListener {
 public:
  static void Show(Profile* profile,
                   crostini::CrostiniInstallerUIDelegate* delegate);
  static CrostiniInstallerView* GetActiveViewForTesting();

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

 private:
  enum class State {
    PROMPT,
    INSTALLING,
    SUCCESS,
    ERROR,
    CANCELING,
    CANCELED,
  };

  explicit CrostiniInstallerView(
      Profile* profile,
      crostini::CrostiniInstallerUIDelegate* delegate);
  ~CrostiniInstallerView() override;

  void OnProgressUpdate(crostini::mojom::InstallerState installing_state,
                        double progress_fraction);
  void OnInstallFinished(crostini::mojom::InstallerError error);
  void OnCanceled();
  void SetMessageLabel();

  State state_ = State::PROMPT;
  crostini::mojom::InstallerState installing_state_;
  Profile* profile_;
  crostini::CrostiniInstallerUIDelegate* delegate_;

  views::ImageView* logo_image_ = nullptr;
  views::Label* big_message_label_ = nullptr;
  views::Label* message_label_ = nullptr;
  views::Link* learn_more_link_ = nullptr;
  views::ImageView* big_image_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;

  base::WeakPtrFactory<CrostiniInstallerView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_INSTALLER_VIEW_H_
