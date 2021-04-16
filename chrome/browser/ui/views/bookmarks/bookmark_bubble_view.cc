// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_observer.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// TODO(pbos): Investigate replacing this with a views-agnostic
// BookmarkBubbleDelegate.
views::BubbleDialogDelegate* BookmarkBubbleView::bookmark_bubble_ = nullptr;

namespace {
constexpr int kBookmarkName = 1;
constexpr int kBookmarkFolder = 2;
}

class BookmarkBubbleView::BookmarkBubbleDelegate
    : public ui::DialogModelDelegate {
 public:
  BookmarkBubbleDelegate(bookmarks::BookmarkBubbleObserver* observer,
                         std::unique_ptr<BubbleSyncPromoDelegate> delegate,
                         Profile* profile,
                         const GURL& url)
      : observer_(observer),
        delegate_(std::move(delegate)),
        profile_(profile),
        url_(url) {}

  void RemoveBookmark() {
    base::RecordAction(UserMetricsAction("BookmarkBubble_Unstar"));
    should_apply_edits_ = false;
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(profile_);
    const bookmarks::BookmarkNode* node =
        model->GetMostRecentlyAddedUserNodeForURL(url_);
    if (node)
      model->Remove(node);
  }

  void OnWindowClosing() {
    if (should_apply_edits_)
      ApplyEdits();
    bookmark_bubble_ = nullptr;
    if (observer_)
      observer_->OnBookmarkBubbleHidden();
  }

  void OnEditButton(const ui::Event& event) {
    base::RecordAction(UserMetricsAction("BookmarkBubble_Edit"));
    ShowEditor();
  }

  void ShowEditor() {
    DCHECK(dialog_model()->host());
    const bookmarks::BookmarkNode* node =
        BookmarkModelFactory::GetForBrowserContext(profile_)
            ->GetMostRecentlyAddedUserNodeForURL(url_);
    DCHECK(bookmark_bubble_->anchor_widget());
    gfx::NativeWindow native_parent =
        bookmark_bubble_->anchor_widget()->GetNativeWindow();
    DCHECK(native_parent);

    Profile* const profile = profile_;
    // Note that closing the dialog with |should_apply_edits_| still true will
    // synchronously save any pending changes.
    dialog_model()->host()->Close();

    if (node && native_parent) {
      BookmarkEditor::Show(native_parent, profile,
                           BookmarkEditor::EditDetails::EditNode(node),
                           BookmarkEditor::SHOW_TREE);
    }
  }

  void OnComboboxAction() {
    if (dialog_model()
                ->GetComboboxByUniqueId(kBookmarkFolder)
                ->selected_index() +
            1 ==
        GetFolderModel()->GetItemCount()) {
      base::RecordAction(UserMetricsAction("BookmarkBubble_EditFromCombobox"));
      ShowEditor();
    }
  }

  void ApplyEdits() {
    DCHECK(should_apply_edits_);
    // Set this to make sure we don't attempt to apply edits again.
    should_apply_edits_ = false;

    bookmarks::BookmarkModel* const model =
        BookmarkModelFactory::GetForBrowserContext(profile_);
    const bookmarks::BookmarkNode* node =
        model->GetMostRecentlyAddedUserNodeForURL(url_);
    if (!node)
      return;
    const std::u16string new_title =
        dialog_model()->GetTextfieldByUniqueId(kBookmarkName)->text();
    if (new_title != node->GetTitle()) {
      model->SetTitle(node, new_title);
      base::RecordAction(
          UserMetricsAction("BookmarkBubble_ChangeTitleInBubble"));
    }

    GetFolderModel()->MaybeChangeParent(
        node, dialog_model()
                  ->GetComboboxByUniqueId(kBookmarkFolder)
                  ->selected_index());
  }

  RecentlyUsedFoldersComboModel* GetFolderModel() {
    DCHECK(dialog_model());
    return static_cast<RecentlyUsedFoldersComboModel*>(
        dialog_model()
            ->GetComboboxByUniqueId(kBookmarkFolder)
            ->combobox_model());
  }

  BubbleSyncPromoDelegate* delegate() { return delegate_.get(); }

 private:
  bookmarks::BookmarkBubbleObserver* const observer_;
  std::unique_ptr<BubbleSyncPromoDelegate> delegate_;
  Profile* const profile_;
  const GURL url_;

  bool should_apply_edits_ = true;
};

// static
void BookmarkBubbleView::ShowBubble(
    views::View* anchor_view,
    views::Button* highlighted_button,
    bookmarks::BookmarkBubbleObserver* observer,
    std::unique_ptr<BubbleSyncPromoDelegate> delegate,
    Profile* profile,
    const GURL& url,
    bool already_bookmarked) {
  if (bookmark_bubble_)
    return;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  BubbleSyncPromoDelegate* const delegate_ptr = delegate.get();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  const bookmarks::BookmarkNode* bookmark_node =
      bookmark_model->GetMostRecentlyAddedUserNodeForURL(url);

  auto bubble_delegate_unique = std::make_unique<BookmarkBubbleDelegate>(
      observer, std::move(delegate), profile, url);
  BookmarkBubbleDelegate* bubble_delegate = bubble_delegate_unique.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(
              already_bookmarked ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARK
                                 : IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED))
          .SetWindowClosingCallback(
              base::BindOnce(&BookmarkBubbleDelegate::OnWindowClosing,
                             base::Unretained(bubble_delegate)))
          .AddOkButton(base::BindOnce(&BookmarkBubbleDelegate::ApplyEdits,
                                      base::Unretained(bubble_delegate)),
                       l10n_util::GetStringUTF16(IDS_DONE))
          .AddCancelButton(
              base::BindOnce(&BookmarkBubbleDelegate::RemoveBookmark,
                             base::Unretained(bubble_delegate)),
              l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK),
              ui::DialogModelButton::Params().AddAccelerator(
                  ui::Accelerator(ui::VKEY_R, ui::EF_ALT_DOWN)))
          .AddDialogExtraButton(
              base::BindRepeating(&BookmarkBubbleDelegate::OnEditButton,
                                  base::Unretained(bubble_delegate)),
              l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_OPTIONS),
              ui::DialogModelButton::Params().AddAccelerator(
                  ui::Accelerator(ui::VKEY_E, ui::EF_ALT_DOWN)))
          .AddTextfield(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_NAME_LABEL),
              bookmark_node->GetTitle(),
              ui::DialogModelTextfield::Params()
                  .SetUniqueId(kBookmarkName)
                  .SetAccessibleName(l10n_util::GetStringUTF16(
                      IDS_BOOKMARK_AX_BUBBLE_NAME_LABEL)))
          .AddCombobox(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_FOLDER_LABEL),
              std::make_unique<RecentlyUsedFoldersComboModel>(
                  bookmark_model,
                  bookmark_model->GetMostRecentlyAddedUserNodeForURL(url)),
              ui::DialogModelCombobox::Params()
                  .SetUniqueId(kBookmarkFolder)
                  .SetCallback(base::BindRepeating(
                      &BookmarkBubbleDelegate::OnComboboxAction,
                      base::Unretained(bubble_delegate))))
          .SetInitiallyFocusedField(kBookmarkName)
          .Build();

  // views:: land below, there's no agnostic reference to arrow / anchors /
  // bubbles.
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  bookmark_bubble_ = bubble.get();
  if (highlighted_button)
    bubble->SetHighlightedButton(highlighted_button);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (SyncPromoUI::ShouldShowSyncPromo(profile)) {
    // TODO(pbos): Consider adding model support for footnotes so that this does
    // not need to be tied to views.
    // TODO(pbos): Consider updating ::SetFootnoteView so that it can resize the
    // widget to account for it.
    bubble->SetFootnoteView(std::make_unique<DiceBubbleSyncPromoView>(
        profile, delegate_ptr,
        signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
        IDS_BOOKMARK_DICE_PROMO_SYNC_MESSAGE,
        /*dice_signin_button_prominent=*/false));
  }
#endif

  views::Widget* const widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  widget->Show();

  chrome::RecordDialogCreation(chrome::DialogIdentifier::BOOKMARK);

  if (observer) {
    observer->OnBookmarkBubbleShown(
        BookmarkModelFactory::GetForBrowserContext(profile)
            ->GetMostRecentlyAddedUserNodeForURL(url));
  }
}

// static
void BookmarkBubbleView::Hide() {
  if (bookmark_bubble_)
    bookmark_bubble_->GetWidget()->Close();
}
