// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_combined_selector_view.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {

// Group ID for radio buttons.
constexpr int kGroupId = 1327;

// The desired width and height in pixels for the Flux UI account avatar.
constexpr int kFluxFederatedAvatarSize = 28;
constexpr int kFluxPasswordIconSize = 20;

// The horizontal and vertical padding in pixels for the Flux UI account row.
constexpr int kHorizontalPadding = 16;
constexpr int kVerticalPadding = 8;

// The maximum number of items to show without scrolling. Used to calculate the
// scrollable area.
constexpr int kMaxVisibleItems = 3;
constexpr int kItemHeight = 72;

class PasswordCombinedSelectorViewWrapper : public views::View {
  METADATA_HEADER(PasswordCombinedSelectorViewWrapper, views::View)
};

BEGIN_METADATA(PasswordCombinedSelectorViewWrapper)
END_METADATA

class PasswordCombinedSelectorRowView : public AccountAvatarFetcherDelegate,
                                        public views::TableLayoutView {
  METADATA_HEADER(PasswordCombinedSelectorRowView, views::TableLayoutView)
 public:
  PasswordCombinedSelectorRowView(
      const std::u16string& username,
      const std::vector<std::u16string>& details,
      bool show_radio_button,
      bool is_federated,
      const GURL& icon_url,
      network::mojom::URLLoaderFactory* loader_factory,
      const url::Origin& initiator,
      base::RepeatingCallback<void(views::RadioButton*)> callback)
      : callback_(callback) {
    const int horizontal_padding = show_radio_button ? kHorizontalPadding : 0;
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(kVerticalPadding, horizontal_padding)));

    const int icon_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_RELATED_LABEL_HORIZONTAL);

    // Define columns: Icon, Padding, Text
    AddColumn(views::LayoutAlignment::kCenter, views::LayoutAlignment::kCenter,
              views::TableLayout::kFixedSize,
              views::TableLayout::ColumnSize::kFixed, kFluxFederatedAvatarSize,
              0);
    AddPaddingColumn(views::TableLayout::kFixedSize, icon_padding);
    AddColumn(views::LayoutAlignment::kStretch,
              views::LayoutAlignment::kStretch, 1.0f,
              views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

    if (show_radio_button) {
      AddPaddingColumn(views::TableLayout::kFixedSize, icon_padding);
      AddColumn(views::LayoutAlignment::kCenter,
                views::LayoutAlignment::kCenter, views::TableLayout::kFixedSize,
                views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
    }

    AddRows(1, views::TableLayout::kFixedSize);

    // Icon
    if (is_federated) {
      auto image_view = std::make_unique<CircularImageView>(
          gfx::Size(kFluxFederatedAvatarSize, kFluxFederatedAvatarSize));
      image_view_ = image_view.get();
      gfx::Image image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE);
      UpdateAvatar(image.AsImageSkia());

      if (icon_url.is_valid()) {
        // Fetch the actual avatar.
        AccountAvatarFetcher* fetcher =
            new AccountAvatarFetcher(icon_url, weak_ptr_factory_.GetWeakPtr());
        fetcher->Start(loader_factory, initiator);
      }
      AddChildView(std::move(image_view));
    } else {
      AddChildView(
          std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
              GooglePasswordManagerVectorIcon(), ui::kColorIcon,
              kFluxPasswordIconSize)));
    }

    // Text Stack
    auto* text_container =
        AddChildView(std::make_unique<views::TableLayoutView>());
    text_container->AddColumn(views::LayoutAlignment::kStart,
                              views::LayoutAlignment::kCenter, 1.0f,
                              views::TableLayout::ColumnSize::kFixed, 0, 0);
    text_container->AddRows(1 + details.size(), views::TableLayout::kFixedSize);

    text_container->AddChildView(
        std::make_unique<views::Label>(username, views::style::CONTEXT_LABEL,
                                       views::style::STYLE_BODY_3_MEDIUM));
    for (const auto& detail : details) {
      text_container->AddChildView(std::make_unique<views::Label>(
          detail, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_4));
    }

    // Radio Button
    if (show_radio_button) {
      radio_button_ = AddChildView(
          std::make_unique<views::RadioButton>(std::u16string(), kGroupId));
      std::u16string acc_name = username;
      for (const auto& detail : details) {
        acc_name += u"\n" + detail;
      }
      radio_button_->GetViewAccessibility().SetName(acc_name);
      radio_button_->SetCallback(base::BindRepeating(
          &PasswordCombinedSelectorRowView::OnRadioButtonPressed,
          base::Unretained(this)));
    }
  }

  void SetChecked(bool checked) {
    if (radio_button_) {
      radio_button_->SetChecked(checked);
    }
  }

  views::RadioButton* GetRadioButton() { return radio_button_; }

  // AccountAvatarFetcherDelegate:
  void UpdateAvatar(const gfx::ImageSkia& image) override {
    if (!image_view_) {
      return;
    }
    gfx::ImageSkia skia_image = image;
    gfx::Size size = skia_image.size();
    if (size.height() != size.width()) {
      gfx::Rect target(size);
      int side = std::min(size.height(), size.width());
      target.ClampToCenteredSize(gfx::Size(side, side));
      skia_image = gfx::ImageSkiaOperations::ExtractSubset(skia_image, target);
    }
    image_view_->SetImage(ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateResizedImage(
            skia_image, skia::ImageOperations::RESIZE_BEST,
            gfx::Size(kFluxFederatedAvatarSize, kFluxFederatedAvatarSize))));
  }

  // views::TableLayoutView:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (radio_button_ && event.IsOnlyLeftMouseButton()) {
      const gfx::Point center = radio_button_->GetLocalBounds().CenterPoint();
      ui::MouseEvent synthetic_press_event(
          ui::EventType::kMousePressed, center, center, event.time_stamp(),
          event.flags(), event.changed_button_flags());
      radio_button_->OnMousePressed(synthetic_press_event);
      return true;
    }
    return views::TableLayoutView::OnMousePressed(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (radio_button_ && event.IsOnlyLeftMouseButton()) {
      const gfx::Point center = radio_button_->GetLocalBounds().CenterPoint();
      ui::MouseEvent synthetic_release_event(
          ui::EventType::kMouseReleased, center, center, event.time_stamp(),
          event.flags(), event.changed_button_flags());
      radio_button_->OnMouseReleased(synthetic_release_event);
      return;
    }
    views::TableLayoutView::OnMouseReleased(event);
  }

 private:
  void OnRadioButtonPressed() {
    if (radio_button_ && radio_button_->GetChecked()) {
      callback_.Run(radio_button_);
    }
  }

  base::RepeatingCallback<void(views::RadioButton*)> callback_;
  // AccountAvatarFetcherDelegate:
  raw_ptr<views::ImageView> image_view_ = nullptr;
  raw_ptr<views::RadioButton> radio_button_ = nullptr;

  base::WeakPtrFactory<PasswordCombinedSelectorRowView> weak_ptr_factory_{this};
};

BEGIN_METADATA(PasswordCombinedSelectorRowView)
END_METADATA

}  // namespace

PasswordCombinedSelectorView::PasswordCombinedSelectorView(
    CredentialManagerDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetModalType(ui::mojom::ModalType::kChild);
  set_margins(views::LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
}

PasswordCombinedSelectorView::~PasswordCombinedSelectorView() = default;

void PasswordCombinedSelectorView::ShowAccountChooser() {
  InitWindow();
  DCHECK(!widget_);
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget_ = base::WrapUnique(
      constrained_window::ShowWebModalDialogViews(this, web_contents_));
}

void PasswordCombinedSelectorView::ControllerGone() {
  selected_form_ = nullptr;
  web_contents_ = nullptr;
  controller_ = nullptr;
  if (widget_) {
    widget_->Close();
  }
}

std::u16string PasswordCombinedSelectorView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_SIGN_IN_TO_WEBSITE_DIALOG_TITLE,
      url_formatter::FormatOriginForSecurityDisplay(
          controller_->GetOrigin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

bool PasswordCombinedSelectorView::ShouldShowCloseButton() const {
  return false;
}

ui::mojom::ModalType PasswordCombinedSelectorView::GetModalType() const {
  return ui::mojom::ModalType::kChild;
}

void PasswordCombinedSelectorView::WindowClosing() {
  radio_buttons_.clear();
  selected_form_ = nullptr;
  if (controller_) {
    // Determine the controller to notify, but clear the pointer before calling
    // OnCloseDialog, as that may destroy the controller.
    auto* controller = controller_.get();
    controller_ = nullptr;
    controller->OnCloseDialog();
  }
}

bool PasswordCombinedSelectorView::Accept() {
  if (controller_ && selected_form_) {
    controller_->OnChooseCredentials(
        *selected_form_,
        password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  }
  // Closed by the controller.
  return false;
}

void PasswordCombinedSelectorView::InitWindow() {
  auto main_view = std::make_unique<PasswordCombinedSelectorViewWrapper>();
  main_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* scroll_view =
      main_view->AddChildView(std::make_unique<views::ScrollView>());

  auto wrapper = std::make_unique<views::View>();
  wrapper->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      /*between_child_spacing=*/4));

  const auto& forms = controller_->GetLocalForms();
  bool show_radio_buttons = forms.size() > 1;
  radio_buttons_.clear();

  if (show_radio_buttons) {
    wrapper->AddChildView(std::make_unique<views::Separator>());
  }

  int i = 0;
  for (const auto& form : forms) {
    if (i > 0) {
      wrapper->AddChildView(std::make_unique<views::Separator>());
    }

    auto labels = GetCredentialLabelsForAccountChooser(*form);
    std::u16string username = labels.first;
    std::vector<std::u16string> details;

    // Use subtitle for details if present
    if (!labels.second.empty()) {
      for (const auto& line :
           base::SplitString(labels.second, u"\n", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY)) {
        details.push_back(line);
      }
    }
    if (password_manager_util::GetMatchType(*form) !=
        password_manager_util::GetLoginMatchType::kExact) {
      details.push_back(url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(form->url),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
    }
    if (!form->IsFederatedCredential()) {
      details.push_back(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_PASSWORD_FROM_GOOGLE_PASSWORD_MANAGER));
    }

    auto* row =
        wrapper->AddChildView(std::make_unique<PasswordCombinedSelectorRowView>(
            username, details, show_radio_buttons,
            form->IsFederatedCredential(), form->icon_url,
            GetURLLoaderForMainFrame(web_contents_).get(),
            web_contents_->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
            base::BindRepeating(
                &PasswordCombinedSelectorView::OnRadioButtonClicked,
                base::Unretained(this), form.get())));

    if (!selected_form_) {
      selected_form_ = form.get();
      row->SetChecked(true);
    }

    if (row->GetRadioButton()) {
      radio_buttons_.push_back(row->GetRadioButton());
    }
    i++;
  }

  if (show_radio_buttons) {
    wrapper->AddChildView(std::make_unique<views::Separator>());
  }

  scroll_view->ClipHeightTo(0, kMaxVisibleItems * kItemHeight);
  scroll_view->SetContents(std::move(wrapper));

  SetContentsView(std::move(main_view));
}

void PasswordCombinedSelectorView::OnRadioButtonClicked(
    const password_manager::PasswordForm* form,
    views::RadioButton* radio_button) {
  if (!controller_) {
    return;
  }
  selected_form_ = form;
  for (views::RadioButton* rb : radio_buttons_) {
    if (rb != radio_button) {
      rb->SetChecked(false);
    }
  }
}
