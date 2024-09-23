// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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
#include "ui/views/view.h"

namespace {

const int kScrollAreaHeight = 192;

// This container has the right Layout() impl to use within a ScrollView.
class ScrollableView : public views::View {
  METADATA_HEADER(ScrollableView, views::View)

 public:
  ScrollableView() = default;
  ScrollableView(const ScrollableView&) = delete;
  ScrollableView& operator=(const ScrollableView&) = delete;
  ~ScrollableView() override = default;

  void Layout(PassKey) override;
};

void ScrollableView::Layout(PassKey) {
  gfx::Size pref = GetPreferredSize();
  int width = pref.width();
  int height = pref.height();
  if (parent()) {
    width = parent()->width();
    height = std::max(parent()->height(), height);
  }
  SetBounds(x(), y(), width, height);

  LayoutSuperclass<views::View>(this);
}

BEGIN_METADATA(ScrollableView)
END_METADATA

}  // namespace

MediaGalleriesDialogViews::MediaGalleriesDialogViews(
    MediaGalleriesDialogController* controller)
    : controller_(controller),
      contents_(new views::View()),
      auxiliary_button_(nullptr),
      confirm_available_(false),
      accepted_(false) {
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_->GetAcceptButtonText());
  SetAcceptCallback(base::BindOnce(
      [](MediaGalleriesDialogViews* dialog) { dialog->accepted_ = true; },
      base::Unretained(this)));
  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(false);
  SetTitle(controller_->GetHeader());
  SetOwnedByWidget(false);
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](MediaGalleriesDialogViews* dialog) {
        dialog->controller_->DialogFinished(dialog->accepted_);
      },
      this));

  std::u16string label = controller_->GetAuxiliaryButtonText();
  if (!label.empty()) {
    auxiliary_button_ = SetExtraView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            &MediaGalleriesDialogViews::ButtonPressed, base::Unretained(this),
            base::BindRepeating(
                &MediaGalleriesDialogController::DidClickAuxiliaryButton,
                base::Unretained(controller_))),
        label));
  }

  InitChildViews();
  if (ControllerHasWebContents()) {
    constrained_window::ShowWebModalDialogViews(this,
                                                controller->WebContents());
  }
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
  contents_->RemoveAllChildViews();
  checkbox_map_.clear();

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const auto insets = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl);
  const int vertical_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  contents_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, insets, vertical_padding));

  // Message text.
  auto* subtext = contents_->AddChildView(
      std::make_unique<views::Label>(controller_->GetSubtext()));
  subtext->SetMultiLine(true);
  subtext->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Scrollable area for checkboxes.
  const int small_vertical_padding =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  auto scroll_container = std::make_unique<ScrollableView>();
  scroll_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      small_vertical_padding));
  scroll_container->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(vertical_padding, 0, vertical_padding, 0)));

  std::vector<std::u16string> section_headers =
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
      header->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
          vertical_padding,
          provider->GetInsetsMetric(views::INSETS_DIALOG).left(),
          vertical_padding, 0)));
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
  auto* scroll_view =
      contents_->AddChildView(views::ScrollView::CreateScrollViewWithBorder());
  scroll_view->SetContents(std::move(scroll_container));
  const int dialog_content_width = views::Widget::GetLocalizedContentsWidth(
      IDS_MEDIA_GALLERIES_DIALOG_CONTENT_WIDTH_CHARS);
  scroll_view->SetPreferredSize(
      gfx::Size(dialog_content_width, kScrollAreaHeight));
}

void MediaGalleriesDialogViews::UpdateGalleries() {
  InitChildViews();
  contents_->DeprecatedLayoutImmediately();

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
    std::u16string details = gallery.pref_info.GetGalleryAdditionalDetails();
    iter->second->secondary_text()->SetText(details);
    iter->second->secondary_text()->SetVisible(details.length() > 0);
    return false;
  }

  auto* gallery_view =
      container->AddChildView(std::make_unique<MediaGalleryCheckboxView>(
          gallery.pref_info, trailing_vertical_space, this));
  gallery_view->checkbox()->SetCallback(base::BindRepeating(
      &MediaGalleriesDialogViews::ButtonPressed, base::Unretained(this),
      base::BindRepeating(
          [](MediaGalleriesDialogController* controller,
             MediaGalleryPrefId pref_id, views::Checkbox* checkbox) {
            controller->DidToggleEntry(pref_id, checkbox->GetChecked());
          },
          controller_, gallery.pref_info.pref_id, gallery_view->checkbox())));
  gallery_view->checkbox()->SetChecked(gallery.selected);
  checkbox_map_[gallery.pref_info.pref_id] = gallery_view;
  return true;
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
    ui::mojom::DialogButton button) const {
  return button != ui::mojom::DialogButton::kOk || confirm_available_;
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
      GetWidget(), nullptr, gfx::Rect(point.x(), point.y(), 0, 0),
      views::MenuAnchorPosition::kTopLeft, source_type);
}

bool MediaGalleriesDialogViews::ControllerHasWebContents() const {
  return controller_->WebContents() != nullptr;
}

void MediaGalleriesDialogViews::ButtonPressed(base::RepeatingClosure closure) {
  confirm_available_ = true;

  if (ControllerHasWebContents())
    DialogModelChanged();

  closure.Run();
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
