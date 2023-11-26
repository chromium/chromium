// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class StyledLabel;
class ImageButton;
class BubbleDialogDelegate;
class LabelButton;
}  // namespace views

class DownloadBubbleNavigationHandler;
class ParagraphsView;
class DownloadBubblePasswordPromptView;

class DownloadBubbleSecurityView : public views::View,
                                   public download::DownloadItem::Observer {
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

  METADATA_HEADER(DownloadBubbleSecurityView);
  DownloadBubbleSecurityView(
      Delegate* delegate,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      views::BubbleDialogDelegate* bubble_delegate);
  DownloadBubbleSecurityView(const DownloadBubbleSecurityView&) = delete;
  DownloadBubbleSecurityView& operator=(const DownloadBubbleSecurityView&) =
      delete;
  ~DownloadBubbleSecurityView() override;

  // Update the security view when a subpage is opened for a particular
  // download. Initializes this view, and associates it with `model`'s download.
  // If it is already associated with the same download, this will update the
  // view if the danger type has changed since the last time it was initialized.
  // It is not an error to initialize this with a download when it is already
  // initialized, either with the same download or a different download.
  void InitializeForDownload(DownloadUIModel& model);

  // Returns this to an uninitialized state, where this is not associated with
  // a particular download. Called when navigating away from the security view.
  void Reset();

  // Whether this view is properly associated with a download. The rest of the
  // public method calls on this view do not make sense if not initialized.
  bool IsInitialized() const;

  // Update the view after it is visible, in particular asking for focus and
  // announcing accessibility text. Must be initialized when called.
  void UpdateAccessibilityTextAndFocus();

  // download::DownloadItem::Observer implementation
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadRemoved(download::DownloadItem* download) override;

  // |is_secondary_button| checks if the command/action originated from the
  // secondary button. Returns whether the dialog should close due to this
  // command.
  bool ProcessButtonClick(DownloadCommands::Command command,
                          bool is_secondary_button);

  const offline_items_collection::ContentId& content_id() const {
    return content_id_;
  }

  void SetUIInfoForTesting(const DownloadUIModel::BubbleUIInfo& ui_info);

 private:
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

  // `old_danger_type` is the previous danger type before the update, which is
  // used for debugging only.
  void UpdateViews(
      download::DownloadDangerType old_danger_type =
          download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  void UpdateHeader();
  void UpdateIconAndText();
  void UpdateSecondaryIconAndText();
  // Updates the subpage button. Setting initial state and color for enabled
  // state, if it is a secondary button.
  void UpdateButton(DownloadUIModel::BubbleUIInfo::SubpageButton button,
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

  // Must outlive this.
  const raw_ptr<Delegate> delegate_;

  // Following 4 fields are cached when the download/model is updated.

  // ContentId of the download this refers to.
  offline_items_collection::ContentId content_id_;
  // UI info at the last time this was created/updated.
  DownloadUIModel::BubbleUIInfo ui_info_;
  // The text for the title (i.e. filename) that this view was last
  // created/updated with.
  std::u16string title_text_;
  // Tracks the danger type of the model when it was last created/updated. Used
  // to determine whether a given model update has changed the danger type.
  download::DownloadDangerType danger_type_ =
      download::DOWNLOAD_DANGER_TYPE_MAX;

  base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;
  raw_ptr<views::BubbleDialogDelegate, DanglingUntriaged> bubble_delegate_ =
      nullptr;

  raw_ptr<views::LabelButton, DanglingUntriaged> secondary_button_ = nullptr;
  raw_ptr<views::StyledLabel> title_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<ParagraphsView> paragraphs_ = nullptr;
  raw_ptr<views::ImageView> secondary_icon_ = nullptr;
  raw_ptr<views::StyledLabel> secondary_styled_label_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  // TODO(chlily): Implement deep_scanning_link_ as a learn_more_link_.
  raw_ptr<views::StyledLabel> deep_scanning_link_ = nullptr;
  raw_ptr<views::StyledLabel> learn_more_link_ = nullptr;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  raw_ptr<DownloadBubblePasswordPromptView> password_prompt_ = nullptr;

  // Records the last time this was shown or updated for a new download. Used
  // for metrics.
  absl::optional<base::Time> warning_time_;
  // Tracks whether metrics were logged for this impression, to avoid
  // double-logging.
  bool did_log_action_ = false;

  // Observation of the download item this refers to. Only observes while this
  // is associated with a download item.
  base::ScopedObservation<download::DownloadItem,
                          download::DownloadItem::Observer>
      download_item_observation_{this};

  base::WeakPtrFactory<DownloadBubbleSecurityView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
