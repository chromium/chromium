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
#include "base/strings/string_util.h"
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

// A custom radio button that allows for group management and focus handling
// across different parent views. This is necessary because the default
// views::RadioButton assumes all buttons in a group share the same parent.
class PasswordCombinedSelectorRadioButton : public views::RadioButton {
  METADATA_HEADER(PasswordCombinedSelectorRadioButton, views::RadioButton)
 public:
  explicit PasswordCombinedSelectorRadioButton(
      PasswordCombinedSelectorRadioButtonDelegate* delegate,
      int index)
      : views::RadioButton(u"", kGroupId), delegate_(delegate), index_(index) {}

  views::View* GetSelectedViewForGroup(int group) override {
    Views views;
    GetRadioButtonsInList(group, &views);

    const auto i = std::ranges::find_if(views, [](const views::View* view) {
      return static_cast<const PasswordCombinedSelectorRadioButton*>(view)
          ->GetChecked();
    });
    return (i == views.cend()) ? nullptr : *i;
  }

  void SetChecked(bool checked) override {
    if (checked == RadioButton::GetChecked()) {
      return;
    }
    if (checked) {
      Views other;
      GetRadioButtonsInList(GetGroup(), &other);
      for (views::View* peer : other) {
        if (peer != this &&
            IsViewClass<PasswordCombinedSelectorRadioButton>(peer)) {
          static_cast<PasswordCombinedSelectorRadioButton*>(peer)->SetChecked(
              false);
        }
      }
      delegate_->OnRadioButtonChecked(index_);
      RequestFocus();
    }
    views::Checkbox::SetChecked(checked);
  }

 private:
  void GetRadioButtonsInList(int group, Views* views) {
    views::View* row_view = parent();
    if (!row_view) {
      return;
    }
    views::View* list_view = row_view->parent();
    if (!list_view) {
      return;
    }
    list_view->GetViewsInGroup(group, views);
  }

  // Allow the Enter key to be used to select the radio button.
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override {
    return event.key_code() == ui::VKEY_RETURN
               ? false
               : RadioButton::SkipDefaultKeyEventProcessing(event);
  }

  raw_ptr<PasswordCombinedSelectorRadioButtonDelegate> delegate_;
  const int index_;
};

BEGIN_METADATA(PasswordCombinedSelectorRadioButton)
END_METADATA

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
      PasswordCombinedSelectorRadioButtonDelegate* radio_delegate,
      int index) {
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
      MaybeAddRadioButton(username, details, radio_delegate, index);
    }
  }

  void MaybeAddRadioButton(
      const std::u16string& username,
      const std::vector<std::u16string>& details,
      PasswordCombinedSelectorRadioButtonDelegate* delegate,
      int index) {
    auto radio_button =
        std::make_unique<PasswordCombinedSelectorRadioButton>(delegate, index);
    std::vector<std::u16string> name_parts = {username};
    name_parts.insert(name_parts.end(), details.begin(), details.end());
    radio_button->GetViewAccessibility().SetName(
        base::JoinString(name_parts, u"\n"));
    radio_button_ = AddChildView(std::move(radio_button));
  }

  void SetChecked(bool checked) {
    if (radio_button_) {
      radio_button_->SetChecked(checked);
    }
  }

  bool is_selected() const {
    return radio_button_ && radio_button_->GetChecked();
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
  void RequestFocus() override {
    if (radio_button_) {
      radio_button_->RequestFocus();
    }
  }

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
      RequestFocus();
      return;
    }
    views::TableLayoutView::OnMouseReleased(event);
  }

 private:
  // AccountAvatarFetcherDelegate:
  raw_ptr<views::ImageView> image_view_ = nullptr;
  raw_ptr<PasswordCombinedSelectorRadioButton> radio_button_ = nullptr;

  base::WeakPtrFactory<PasswordCombinedSelectorRowView> weak_ptr_factory_{this};
};

BEGIN_METADATA(PasswordCombinedSelectorRowView)
END_METADATA

class PasswordCombinedSelectorListView : public views::View {
  METADATA_HEADER(PasswordCombinedSelectorListView, views::View)
 public:
  PasswordCombinedSelectorListView(
      CredentialManagerDialogController* controller,
      content::WebContents* web_contents,
      PasswordCombinedSelectorRadioButtonDelegate* delegate) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());

    auto wrapper = std::make_unique<views::View>();
    wrapper->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        /*between_child_spacing=*/4));

    const std::vector<std::unique_ptr<password_manager::PasswordForm>>& forms =
        controller->GetLocalForms();
    bool show_radio_buttons = forms.size() > 1;

    if (show_radio_buttons) {
      wrapper->AddChildView(std::make_unique<views::Separator>());
    }

    int i = 0;
    for (const std::unique_ptr<password_manager::PasswordForm>& form : forms) {
      if (i > 0) {
        wrapper->AddChildView(std::make_unique<views::Separator>());
      }

      std::pair<std::u16string, std::u16string> labels =
          GetCredentialLabelsForAccountChooser(*form);
      std::u16string username = labels.first;
      std::vector<std::u16string> details;

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

      auto* row = wrapper->AddChildView(
          std::make_unique<PasswordCombinedSelectorRowView>(
              username, details, show_radio_buttons,
              form->IsFederatedCredential(), form->icon_url,
              GetURLLoaderForMainFrame(web_contents).get(),
              web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              delegate, i));

      if (i == 0) {
        selected_view_ = row;
      }

      i++;
    }

    wrapper->SetOwnedGroup(kGroupId);

    if (show_radio_buttons) {
      wrapper->AddChildView(std::make_unique<views::Separator>());
    }

    scroll_view->ClipHeightTo(0, kMaxVisibleItems * kItemHeight);
    scroll_view->SetContents(std::move(wrapper));
  }

  void RequestFocus() override {
    if (selected_view_) {
      selected_view_->RequestFocus();
    }
  }

  void SetSelectedView(views::View* view) { selected_view_ = view; }

  void SetSelectedIndex(size_t index) {
    PasswordCombinedSelectorRowView* row = GetRowView(index);
    if (row) {
      row->SetChecked(true);
      selected_view_ = row;
    }
  }

  PasswordCombinedSelectorRowView* GetRowView(size_t index) {
    auto* scroll_view = static_cast<views::ScrollView*>(children()[0]);
    views::View* wrapper = scroll_view->contents();
    const auto& forms = wrapper->children();
    // When there are multiple forms, separators are added between them and
    // around the list, so the actual row views are at odd indices (1, 3, 5...).
    size_t child_index = (forms.size() > 1) ? 1 + 2 * index : index;
    if (child_index >= forms.size()) {
      return nullptr;
    }
    return static_cast<PasswordCombinedSelectorRowView*>(forms[child_index]);
  }

  std::vector<raw_ptr<views::RadioButton>> GetRadioButtons() {
    std::vector<raw_ptr<views::RadioButton>> radio_buttons;
    auto* scroll_view = static_cast<views::ScrollView*>(children()[0]);
    views::View* wrapper = scroll_view->contents();
    for (views::View* child : wrapper->children()) {
      if (views::IsViewClass<PasswordCombinedSelectorRowView>(child)) {
        auto* radio_button =
            static_cast<PasswordCombinedSelectorRowView*>(child)
                ->GetRadioButton();
        if (radio_button) {
          radio_buttons.push_back(radio_button);
        }
      }
    }
    return radio_buttons;
  }

 private:
  raw_ptr<views::View> selected_view_ = nullptr;
};

BEGIN_METADATA(PasswordCombinedSelectorListView)
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
  list_view_ = nullptr;
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
  // The dialog is closed by the controller, so return false here to prevent the
  // default dialog closing logic.
  return false;
}

void PasswordCombinedSelectorView::InitWindow() {
  auto main_view = std::make_unique<PasswordCombinedSelectorViewWrapper>();
  main_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  auto list_view = std::make_unique<PasswordCombinedSelectorListView>(
      controller_.get(), web_contents_, this);
  list_view_ = main_view->AddChildView(std::move(list_view));

  const auto& forms = controller_->GetLocalForms();
  if (!forms.empty()) {
    selected_form_ = forms[0].get();
    static_cast<PasswordCombinedSelectorListView*>(list_view_)
        ->SetSelectedIndex(0);
  }
  radio_buttons_ = static_cast<PasswordCombinedSelectorListView*>(list_view_)
                       ->GetRadioButtons();

  SetContentsView(std::move(main_view));
}

void PasswordCombinedSelectorView::OnRadioButtonChecked(int index) {
  if (!controller_) {
    return;
  }
  const auto& forms = controller_->GetLocalForms();
  if (static_cast<size_t>(index) < forms.size()) {
    selected_form_ = forms[index].get();
  }

  // Update selected_view_ in list_view_ for RequestFocus.
  auto* list_view_ptr =
      static_cast<PasswordCombinedSelectorListView*>(list_view_);
  list_view_ptr->SetSelectedView(list_view_ptr->GetRowView(index));
}
