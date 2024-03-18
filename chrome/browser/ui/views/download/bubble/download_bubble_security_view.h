// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_security_view_info.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class StyledLabel;
class ImageButton;
class BubbleDialogDelegate;
}  // namespace views

class DownloadBubbleNavigationHandler;
class ParagraphsView;
class DownloadBubblePasswordPromptView;

class DownloadBubbleSecurityView
    : public views::View,
      public DownloadBubbleSecurityViewInfoObserver {
  METADATA_HEADER(DownloadBubbleSecurityView, views::View)

 public:
  // Interface allowing this to interact with the download item/model its was
  // created for.
  class Delegate {
   public:
    // Processes a button press on the download with given id, which should
    // execute the given command.
    virtual void ProcessSecuritySubpageButtonPress(
        const offline_items_collection::ContentId& id,
        DownloadCommands::Command command) = 0;

    // Record a warning action on the download with the given id.
    virtual void AddSecuritySubpageWarningActionEvent(
        const offline_items_collection::ContentId& id,
        DownloadItemWarningData::WarningAction action) = 0;

    // Processes the deep scan being pressed, when the given password is
    // provided.
    virtual void ProcessDeepScanPress(
        const offline_items_collection::ContentId& id,
        DownloadItemWarningData::DeepScanTrigger trigger,
        base::optional_ref<const std::string> password) = 0;

    // Processes the local decryption prompt being accepted/ignored.
    virtual void ProcessLocalDecryptionPress(
        const offline_items_collection::ContentId& id,
        base::optional_ref<const std::string> password) = 0;

    // Processes clicks on the in-progress view for local decryption scans.
    virtual void ProcessLocalPasswordInProgressClick(
        const offline_items_collection::ContentId& id,
        DownloadCommands::Command command) = 0;

    // Return whether the download item is an encrypted archive.
    virtual bool IsEncryptedArchive(
        const offline_items_collection::ContentId& id) = 0;

    // Return whether the download item has a previously provided invalid
    // password.
    virtual bool HasPreviousIncorrectPassword(
        const offline_items_collection::ContentId& id) = 0;
  };

  DownloadBubbleSecurityView(
      Delegate* delegate,
      const DownloadBubbleSecurityViewInfo& info,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      views::BubbleDialogDelegate* bubble_delegate);
  DownloadBubbleSecurityView(const DownloadBubbleSecurityView&) = delete;
  DownloadBubbleSecurityView& operator=(const DownloadBubbleSecurityView&) =
      delete;
  ~DownloadBubbleSecurityView() override;

  // Returns this to an uninitialized state, where this is not associated with
  // a particular download. Called when navigating away from the security view.
  void Reset();

  // Whether this view is properly associated with a download. The rest of the
  // public method calls on this view do not make sense if not initialized.
  bool IsInitialized() const;

  // Update the view after it is visible, in particular asking for focus and
  // announcing accessibility text. Must be initialized when called.
  void UpdateAccessibilityTextAndFocus();

  // |is_secondary_button| checks if the command/action originated from the
  // secondary button. Returns whether the dialog should close due to this
  // command.
  bool ProcessButtonClick(DownloadCommands::Command command,
                          bool is_secondary_button);

  // Logs the DISMISS action on the DownloadItemWarningData, if initialized.
  // Should be called when the security view is about to be destroyed.
  void MaybeLogDismiss();

  const offline_items_collection::ContentId& content_id() const;

 private:
  friend class DownloadBubbleSecurityViewTest;
  FRIEND_TEST_ALL_PREFIXES(DownloadBubbleSecurityViewTest,
                           VerifyLogWarningActions);

  // Following method calls require this view to be initialized.
  void BackButtonPressed();
  void AddHeader();
  void CloseBubble();
  void AddIconAndContents();
  void AddSecondaryIconAndText();
  void AddProgressBar();
  void AddPasswordPrompt(views::View* parent);

  void UpdateViews();
  void UpdateHeader();
  void UpdateIconAndText();
  void UpdateSecondaryIconAndText();
  // Updates the subpage button. Setting initial state and color for enabled
  // state, if it is a secondary button.
  void UpdateButton(DownloadBubbleSecurityViewInfo::SubpageButton button,
                    bool is_secondary_button);
  void UpdateButtons();
  void UpdateProgressBar();
  void UpdatePasswordPrompt();

  // Reset fields that increase the width of the bubble.
  void ClearWideFields();

  void RecordWarningActionTime(bool is_secondary_button);

  int GetMinimumBubbleWidth() const;
  // Minimum width for the filename in the title.
  int GetMinimumTitleWidth() const;
  // Minimum width for the subpage summary.
  int GetMinimumLabelWidth() const;

  // Deep scanning is complicated enough that this button click is separate from
  // the others. Returns whether the dialog should close.
  bool ProcessDeepScanClick();

  // Prompting for local archive decryption is complicated enough that handling
  // these button presses is handled separately.
  bool ProcessLocalPasswordDecryptionClick();

  // DownloadBubbleSecurityViewInfoObserver:
  void OnInfoChanged() override;
  void OnContentIdChanged() override;

  // Must outlive this.
  const raw_ptr<Delegate> delegate_;

  // A reference to the info used to populate this class. `info_` will
  // notify `this` about changes that require updates.
  raw_ref<const DownloadBubbleSecurityViewInfo> info_;

  base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;
  raw_ptr<views::BubbleDialogDelegate, DanglingUntriaged> bubble_delegate_ =
      nullptr;

  raw_ptr<views::StyledLabel> title_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<ParagraphsView> paragraphs_ = nullptr;
  raw_ptr<views::ImageView> secondary_icon_ = nullptr;
  raw_ptr<views::StyledLabel> secondary_styled_label_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  raw_ptr<views::StyledLabel> learn_more_link_ = nullptr;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  raw_ptr<DownloadBubblePasswordPromptView> password_prompt_ = nullptr;

  // Records the last time this was shown or updated for a new download. Used
  // for metrics.
  std::optional<base::Time> warning_time_;
  // Tracks whether metrics were logged for this impression, to avoid
  // double-logging.
  bool did_log_action_ = false;

  base::WeakPtrFactory<DownloadBubbleSecurityView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
