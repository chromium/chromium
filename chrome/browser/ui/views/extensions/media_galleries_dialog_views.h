// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_DIALOG_VIEWS_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class LabelButton;
class MenuRunner;
class Widget;
}

class MediaGalleryCheckboxView;

// The media galleries configuration view for Views. It will immediately show
// upon construction.
class MediaGalleriesDialogViews : public MediaGalleriesDialog,
                                  public views::ContextMenuController,
                                  public views::DialogDelegate {
 public:
  explicit MediaGalleriesDialogViews(
      MediaGalleriesDialogController* controller);

  MediaGalleriesDialogViews(const MediaGalleriesDialogViews&) = delete;
  MediaGalleriesDialogViews& operator=(const MediaGalleriesDialogViews&) =
      delete;

  ~MediaGalleriesDialogViews() override;

  // MediaGalleriesDialog:
  void UpdateGalleries() override;

  // views::DialogDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

 private:
  friend class MediaGalleriesDialogTest;
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, InitializeCheckboxes);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, UpdateAdds);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, ForgetDeletes);

  using CheckboxMap = std::map<MediaGalleryPrefId, MediaGalleryCheckboxView*>;

  // MediaGalleriesDialog:
  void AcceptDialogForTesting() override;

  void InitChildViews();

  // Adds a checkbox or updates an existing checkbox. Returns true if a new one
  // was added.
  bool AddOrUpdateGallery(
      const MediaGalleriesDialogController::Entry& gallery,
      views::View* container,
      int trailing_vertical_space);

  void ShowContextMenu(const gfx::Point& point,
                       ui::MenuSourceType source_type,
                       MediaGalleryPrefId id);

  // Whether |controller_| has a valid WebContents or not.
  // In unit tests, it may not.
  bool ControllerHasWebContents() const;

  // Called when a button is pressed; does common preamble, then runs the
  // supplied closure to execute the specific details of the particular button.
  void ButtonPressed(base::RepeatingClosure closure);

  // Callback for MenuRunner.
  void OnMenuClosed();

  raw_ptr<MediaGalleriesDialogController, DanglingUntriaged> controller_;

  // The contents of the dialog. Owned by the view hierarchy, except in tests.
  raw_ptr<views::View, AcrossTasksDanglingUntriaged> contents_;

  // A map from gallery ID to views::Checkbox view.
  CheckboxMap checkbox_map_;

  // Pointer to the controller specific auxiliary button, NULL otherwise.
  // Owned by parent in the dialog views tree.
  raw_ptr<views::LabelButton, AcrossTasksDanglingUntriaged> auxiliary_button_;

  // This tracks whether the confirm button can be clicked. It starts as false
  // if no checkboxes are ticked. After there is any interaction, or some
  // checkboxes start checked, this will be true.
  bool confirm_available_;

  // True if the user has pressed accept.
  bool accepted_;

  std::unique_ptr<views::MenuRunner> context_menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_DIALOG_VIEWS_H_
