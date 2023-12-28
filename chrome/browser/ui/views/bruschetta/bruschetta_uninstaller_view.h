// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BRUSCHETTA_BRUSCHETTA_UNINSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BRUSCHETTA_BRUSCHETTA_UNINSTALLER_VIEW_H_

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Label;
class ProgressBar;
}  // namespace views

namespace bruschetta {
enum class BruschettaResult;
}  // namespace bruschetta

class Profile;

// The Bruschetta uninstaller. Provides a warning to the user and
// uninstalls Bruschetta if the user chooses to do so.
class BruschettaUninstallerView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(BruschettaUninstallerView, views::BubbleDialogDelegateView)

 public:
  BruschettaUninstallerView(const BruschettaUninstallerView&) = delete;
  BruschettaUninstallerView& operator=(const BruschettaUninstallerView&) =
      delete;

  static void Show(Profile* profile, const guest_os::GuestId& guest_id);

  // views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;

  static BruschettaUninstallerView* GetActiveViewForTesting();
  void set_destructor_callback_for_testing(base::OnceClosure callback) {
    destructor_callback_for_testing_.ReplaceClosure(std::move(callback));
  }

 protected:
  // WidgetDelegate overrides
  void OnWidgetInitialized() override;

 private:
  enum class State {
    PROMPT,  // Prompting the user to allow uninstallation.
    ERROR,   // Something unexpected happened.
    UNINSTALLING,
  };

  explicit BruschettaUninstallerView(Profile* profile,
                                     guest_os::GuestId guest_id);
  ~BruschettaUninstallerView() override;

  void HandleError();
  void UninstallBruschettaFinished(bool success);

  State state_ = State::PROMPT;
  raw_ptr<views::Label> message_label_ = nullptr;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;

  raw_ptr<Profile> profile_;
  guest_os::GuestId guest_id_;

  base::ScopedClosureRunner destructor_callback_for_testing_;

  base::WeakPtrFactory<BruschettaUninstallerView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BRUSCHETTA_BRUSCHETTA_UNINSTALLER_VIEW_H_
