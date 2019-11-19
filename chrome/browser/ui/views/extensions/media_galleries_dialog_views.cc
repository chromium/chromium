// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace {

const int kScrollAreaHeight = 192;

// This container has the right Layout() impl to use within a ScrollView.
class ScrollableView : public views::View {
 public:
  ScrollableView() {}
  ~ScrollableView() override {}

  void Layout() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollableView);
};

void ScrollableView::Layout() {
  gfx::Size pref = GetPreferredSize();
  int width = pref.width();
  int height = pref.height();
  if (parent()) {
    width = parent()->width();
    height = std::max(parent()->height(), height);
  }
  SetBounds(x(), y(), width, height);

  views::View::Layout();
}

std::unique_ptr<views::LabelButton> CreateAuxiliaryButton(
    views::ButtonListener* listener,
    const base::string16& label) {
  return label.empty()
             ? nullptr
             : views::MdTextButton::CreateSecondaryUiButton(listener, label);
}

}  // namespace

MediaGalleriesDialogViews::MediaGalleriesDialogViews(
    MediaGalleriesDialogController* controller)
    : controller_(controller),
      contents_(new views::View()),
      auxiliary_button_(nullptr),
      confirm_available_(false),
      accepted_(false) {
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   controller_->GetAcceptButtonText());
  auxiliary_button_ = DialogDelegate::SetExtraView(
      CreateAuxiliaryButton(this, controller_->GetAuxiliaryButtonText()));

  InitChildViews();
  if (ControllerHasWebContents()) {
    constrained_window::ShowWebModalDialogViews(this,
                                                controller->WebContents());
  }
  chrome::RecordDialogCreation(chrome::DialogIdentifier::MEDIA_GALLERIES);
}

MediaGalleriesDialogViews::~MediaGalleriesDialogViews() {
  if (!ControllerHasWebContents())
    delete contents_;
}

void MediaGalleriesDialogViews::AcceptDialogForTesting() {
  accepted_ = true;

  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          controller_->WebContents());
  DCHECK(manager);
  web_modal::WebContentsModalDialogManager::TestApi(manager).CloseAllDialogs();
}

void MediaGalleriesDialogViews::InitChildViews() {
  // Outer dialog layout.
  contents_->RemoveAllChildViews(true);
  checkbox_map_.clear();

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  contents_->SetBorder(views::CreateEmptyBorder(
      provider->GetDialogInsetsForContentType(views::TEXT, views::CONTROL)));

  const int dialog_content_width = views::Widget::GetLocalizedContentsWidth(
      IDS_MEDIA_GALLERIES_DIALOG_CONTENT_WIDTH_CHARS);
  views::GridLayout* layout =
      contents_->SetLayoutManager(std::make_unique<views::GridLayout>());

  int column_set_id = 0;
  views::ColumnSet* columns = layout->AddColumnSet(column_set_id);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     1.0, views::GridLayout::FIXED, dialog_content_width, 0);

  // Message text.
  const int vertical_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  auto subtext = std::make_unique<views::Label>(controller_->GetSubtext());
  subtext->SetMultiLine(true);
  subtext->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Get the height here rather than inline because the order of evaluation for
  // parameters may differ between platforms.
  const int subtext_height = subtext->GetHeightForWidth(dialog_content_width);
  layout->StartRow(views::GridLayout::kFixedSize, column_set_id);
  layout->AddView(std::move(subtext), 1, 1, views::GridLayout::FILL,
                  views::GridLayout::LEADING, dialog_content_width,
                  subtext_height);
  layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_padding);

  // Scrollable area for checkboxes.
  const int small_vertical_padding =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  auto scroll_container = std::make_unique<ScrollableView>();
  scroll_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      small_vertical_padding));
  scroll_container->SetBorder(
      views::CreateEmptyBorder(vertical_padding, 0, vertical_padding, 0));

  std::vector<base::string16> section_headers =
      controller_->GetSectionHeaders();
  for (size_t i = 0; i < section_headers.size(); i++) {
    MediaGalleriesDialogController::Entries entries =
        controller_->GetSectionEntries(i);

    // Header and separator line.
    if (!section_headers[i].empty() && !entries.empty()) {
      scroll_container->AddChildView(std::make_unique<views::Separator>());

      auto header = std::make_unique<views::Label>(section_headers[i]);
      header->SetMultiLine(true);
      header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      header->SetBorder(views::CreateEmptyBorder(
          vertical_padding,
          provider->GetInsetsMetric(views::INSETS_DIALOG).left(),
          vertical_padding, 0));
      scroll_container->AddChildView(std::move(header));
    }

    // Checkboxes.
    MediaGalleriesDialogController::Entries::const_iterator iter;
    for (iter = entries.begin(); iter != entries.end(); ++iter) {
      int spacing = iter + 1 == entries.end() ? small_vertical_padding : 0;
      AddOrUpdateGallery(*iter, scroll_container.get(), spacing);
    }
  }

  confirm_available_ = controller_->IsAcceptAllowed();

  // Add the scrollable area to the outer dialog view. It will squeeze against
  // the title/subtitle and buttons to occupy all available space in the dialog.
  auto scroll_view = views::ScrollView::CreateScrollViewWithBorder();
  scroll_view->SetContents(std::move(scroll_container));
  layout->StartRowWithPadding(1.0, column_set_id, views::GridLayout::kFixedSize,
                              vertical_padding);
  layout->AddView(std::move(scroll_view), 1.0, 1.0, views::GridLayout::FILL,
                  views::GridLayout::FILL, dialog_content_width,
                  kScrollAreaHeight);
}

void MediaGalleriesDialogViews::UpdateGalleries() {
  InitChildViews();
  contents_->Layout();

  if (ControllerHasWebContents())
    DialogModelChanged();
}

bool MediaGalleriesDialogViews::AddOrUpdateGallery(
    const MediaGalleriesDialogController::Entry& gallery,
    views::View* container,
    int trailing_vertical_space) {
  auto iter = checkbox_map_.find(gallery.pref_info.pref_id);
  if (iter != checkbox_map_.end()) {
    views::Checkbox* checkbox = iter->second->checkbox();
    checkbox->SetChecked(gallery.selected);
    checkbox->SetText(gallery.pref_info.GetGalleryDisplayName());
    checkbox->SetTooltipText(gallery.pref_info.GetGalleryTooltip());
    base::string16 details = gallery.pref_info.GetGalleryAdditionalDetails();
    iter->second->secondary_text()->SetText(details);
    iter->second->secondary_text()->SetVisible(details.length() > 0);
    return false;
  }

  MediaGalleryCheckboxView* gallery_view = new MediaGalleryCheckboxView(
      gallery.pref_info, trailing_vertical_space, this, this);
  gallery_view->checkbox()->SetChecked(gallery.selected);
  container->AddChildView(gallery_view);
  checkbox_map_[gallery.pref_info.pref_id] = gallery_view;

  return true;
}

base::string16 MediaGalleriesDialogViews::GetWindowTitle() const {
  return controller_->GetHeader();
}

bool MediaGalleriesDialogViews::ShouldShowCloseButton() const {
  return false;
}

void MediaGalleriesDialogViews::DeleteDelegate() {
  controller_->DialogFinished(accepted_);
}

views::Widget* MediaGalleriesDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* MediaGalleriesDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* MediaGalleriesDialogViews::GetContentsView() {
  return contents_;
}

bool MediaGalleriesDialogViews::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK || confirm_available_;
}

ui::ModalType MediaGalleriesDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool MediaGalleriesDialogViews::Cancel() {
  return true;
}

bool MediaGalleriesDialogViews::Accept() {
  accepted_ = true;
  return true;
}

void MediaGalleriesDialogViews::ButtonPressed(views::Button* sender,
                                              const ui::Event& /* event */) {
  confirm_available_ = true;

  if (ControllerHasWebContents())
    DialogModelChanged();

  if (sender == auxiliary_button_) {
    controller_->DidClickAuxiliaryButton();
    return;
  }

  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (sender == iter->second->checkbox()) {
      controller_->DidToggleEntry(iter->first,
                                  iter->second->checkbox()->GetChecked());
      return;
    }
  }
}

void MediaGalleriesDialogViews::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (iter->second->Contains(source)) {
      ShowContextMenu(point, source_type, iter->first);
      return;
    }
  }
}

void MediaGalleriesDialogViews::ShowContextMenu(const gfx::Point& point,
                                                ui::MenuSourceType source_type,
                                                MediaGalleryPrefId id) {
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      controller_->GetContextMenu(id),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
      base::BindRepeating(&MediaGalleriesDialogViews::OnMenuClosed,
                          base::Unretained(this)));

  context_menu_runner_->RunMenuAt(
      GetWidget(), nullptr,
      gfx::Rect(point.x(), point.y(), views::GridLayout::kFixedSize, 0),
      views::MenuAnchorPosition::kTopLeft, source_type);
}

bool MediaGalleriesDialogViews::ControllerHasWebContents() const {
  return controller_->WebContents() != nullptr;
}

void MediaGalleriesDialogViews::OnMenuClosed() {
  context_menu_runner_.reset();
}

// MediaGalleriesDialogViewsController -----------------------------------------

// static
MediaGalleriesDialog* MediaGalleriesDialog::Create(
    MediaGalleriesDialogController* controller) {
  return new MediaGalleriesDialogViews(controller);
}
