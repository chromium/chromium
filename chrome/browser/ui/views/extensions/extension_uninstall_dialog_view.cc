// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {
class ExtensionUninstallDialogDelegateView;

// Views implementation of the uninstall dialog.
class ExtensionUninstallDialogViews
    : public extensions::ExtensionUninstallDialog {
 public:
  ExtensionUninstallDialogViews(
      Profile* profile,
      gfx::NativeWindow parent,
      extensions::ExtensionUninstallDialog::Delegate* delegate);
  ~ExtensionUninstallDialogViews() override;

  // Called when the ExtensionUninstallDialogDelegate has been destroyed to make
  // sure we invalidate pointers. This object will also be freed.
  void DialogDelegateDestroyed();

  // Forwards the accept and cancels to the delegate.
  void DialogAccepted(bool checkbox_checked);
  void DialogCanceled();

 private:
  void Show() override;

  ExtensionUninstallDialogDelegateView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogViews);
};

// The dialog's view, owned by the views framework.
class ExtensionUninstallDialogDelegateView
    : public views::BubbleDialogDelegateView {
 public:
  // Constructor for view component of dialog. triggering_extension may be null
  // if the uninstall dialog was manually triggered (from chrome://extensions).
  ExtensionUninstallDialogDelegateView(
      ExtensionUninstallDialogViews* dialog_view,
      ToolbarActionView* anchor_view,
      const extensions::Extension* extension,
      const extensions::Extension* triggering_extension,
      const gfx::ImageSkia* image);
  ~ExtensionUninstallDialogDelegateView() override;

  // Called when the ExtensionUninstallDialog has been destroyed to make sure
  // we invalidate pointers.
  void DialogDestroyed() { dialog_ = nullptr; }

 private:
  // views::View:
  const char* GetClassName() const override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override {
    return is_bubble_ ? ui::MODAL_TYPE_NONE : ui::MODAL_TYPE_WINDOW;
  }

  ExtensionUninstallDialogViews* dialog_;
  const bool is_bubble_;

  views::Label* heading_;
  views::Checkbox* checkbox_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogDelegateView);
};

ExtensionUninstallDialogViews::ExtensionUninstallDialogViews(
    Profile* profile,
    gfx::NativeWindow parent,
    extensions::ExtensionUninstallDialog::Delegate* delegate)
    : extensions::ExtensionUninstallDialog(profile, parent, delegate) {}

ExtensionUninstallDialogViews::~ExtensionUninstallDialogViews() {
  // Close the widget (the views framework will delete view_).
  if (view_) {
    view_->DialogDestroyed();
    view_->GetWidget()->CloseNow();
  }
}

void ExtensionUninstallDialogViews::Show() {
  BrowserView* const browser_view =
      parent() ? BrowserView::GetBrowserViewForNativeWindow(parent()) : nullptr;
  ToolbarActionView* anchor_view = nullptr;
  ExtensionsToolbarContainer* const container =
      browser_view ? browser_view->toolbar_button_provider()
                         ->GetExtensionsToolbarContainer()
                   : nullptr;
  if (container) {
    anchor_view = container->GetViewForId(extension()->id());
  } else if (browser_view &&
             !base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
    BrowserActionsContainer* const browser_actions_container =
        browser_view->toolbar_button_provider()->GetBrowserActionsContainer();
    ToolbarActionView* const reference_view =
        browser_actions_container
            ? browser_actions_container->GetViewForId(extension()->id())
            : nullptr;
    if (reference_view && reference_view->GetVisible())
      anchor_view = reference_view;
  }
  view_ = new ExtensionUninstallDialogDelegateView(
      this, anchor_view, extension(), triggering_extension(), &icon());
  if (anchor_view) {
    if (container) {
      container->ShowWidgetForExtension(
          views::BubbleDialogDelegateView::CreateBubble(view_),
          extension()->id());
    } else {
      DCHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
      views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
    }
  } else {
    constrained_window::CreateBrowserModalDialogViews(view_, parent())->Show();
  }
}

void ExtensionUninstallDialogViews::DialogDelegateDestroyed() {
  // Checks view_ to ensure OnDialogClosed() will not be called twice.
  if (view_) {
    view_ = nullptr;
    OnDialogClosed(CLOSE_ACTION_CANCELED);
  }
}

void ExtensionUninstallDialogViews::DialogAccepted(bool checkbox_checked) {
  // The widget gets destroyed when the dialog is accepted.
  DCHECK(view_);
  view_->DialogDestroyed();
  view_ = nullptr;

  OnDialogClosed(checkbox_checked ? CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED
                                  : CLOSE_ACTION_UNINSTALL);
}

void ExtensionUninstallDialogViews::DialogCanceled() {
  // The widget gets destroyed when the dialog is canceled.
  DCHECK(view_);
  view_->DialogDestroyed();
  view_ = nullptr;
  OnDialogClosed(CLOSE_ACTION_CANCELED);
}

ExtensionUninstallDialogDelegateView::ExtensionUninstallDialogDelegateView(
    ExtensionUninstallDialogViews* dialog_view,
    ToolbarActionView* anchor_view,
    const extensions::Extension* extension,
    const extensions::Extension* triggering_extension,
    const gfx::ImageSkia* image)
    : BubbleDialogDelegateView(anchor_view,
                               anchor_view ? views::BubbleBorder::TOP_RIGHT
                                           : views::BubbleBorder::NONE),
      dialog_(dialog_view),
      is_bubble_(anchor_view != nullptr),
      checkbox_(nullptr) {
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON));
  SetIcon(gfx::ImageSkiaOperations::CreateResizedImage(
      *image, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
      gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                extension_misc::EXTENSION_ICON_SMALL)));
  SetShowCloseButton(false);
  SetShowIcon(true);
  SetTitle(l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_TITLE,
                                      base::UTF8ToUTF16(extension->name())));

  SetAcceptCallback(base::BindOnce(
      [](ExtensionUninstallDialogDelegateView* view) {
        if (view->dialog_) {
          view->dialog_->DialogAccepted(view->checkbox_ &&
                                        view->checkbox_->GetChecked());
        }
      },
      base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      [](ExtensionUninstallDialogDelegateView* view) {
        if (view->dialog_)
          view->dialog_->DialogCanceled();
      },
      base::Unretained(this)));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  // Add margins for the icon plus the icon-title padding so that the dialog
  // contents align with the title text.
  set_margins(
      margins() +
      gfx::Insets(0, margins().left() + extension_misc::EXTENSION_ICON_SMALL, 0,
                  0));

  if (triggering_extension) {
    heading_ = new views::Label(
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PROMPT_UNINSTALL_TRIGGERED_BY_EXTENSION,
            base::UTF8ToUTF16(triggering_extension->name())),
        views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    heading_->SetAllowCharacterBreak(true);
    AddChildView(heading_);
  }

  if (dialog_->ShouldShowCheckbox()) {
    checkbox_ = new views::Checkbox(dialog_->GetCheckboxLabel());
    checkbox_->SetMultiLine(true);
    AddChildView(checkbox_);
  }

  if (anchor_view)
    anchor_view->AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTENSION_UNINSTALL);
}

ExtensionUninstallDialogDelegateView::~ExtensionUninstallDialogDelegateView() {
  // If we're here, 2 things could have happened. Either the user closed the
  // dialog nicely and one of the installed/canceled methods has been called
  // (in which case dialog_ will be null), *or* neither of them have been
  // called and we are being forced closed by our parent widget. In this case,
  // we need to make sure to notify dialog_ not to call us again, since we're
  // about to be freed by the Widget framework.
  if (dialog_)
    dialog_->DialogDelegateDestroyed();

  // If there is still a toolbar action view its ink drop should be deactivated
  // when the uninstall dialog goes away. This lookup is repeated as the dialog
  // can go away during dialog's lifetime (especially when uninstalling).
  views::View* anchor_view = GetAnchorView();
  if (anchor_view) {
    static_cast<ToolbarActionView*>(anchor_view)
        ->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  }
}

const char* ExtensionUninstallDialogDelegateView::GetClassName() const {
  return "ExtensionUninstallDialogDelegateView";
}

gfx::Size ExtensionUninstallDialogDelegateView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        is_bubble_ ? DISTANCE_BUBBLE_PREFERRED_WIDTH
                                   : DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

}  // namespace

// static
std::unique_ptr<extensions::ExtensionUninstallDialog>
extensions::ExtensionUninstallDialog::Create(Profile* profile,
                                             gfx::NativeWindow parent,
                                             Delegate* delegate) {
  return CreateViews(profile, parent, delegate);
}

// static
std::unique_ptr<extensions::ExtensionUninstallDialog>
extensions::ExtensionUninstallDialog::CreateViews(Profile* profile,
                                                  gfx::NativeWindow parent,
                                                  Delegate* delegate) {
  return std::make_unique<ExtensionUninstallDialogViews>(profile, parent,
                                                         delegate);
}
