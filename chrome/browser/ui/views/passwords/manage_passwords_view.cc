// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kIconSize = 16;
// TODO(crbug.com/1408790): Row height should be computed from line/icon heights
// and desired paddings, instead of a fixed value to account for font size
// changes.
// The height of the row in the table layout displaying the password details.
constexpr int kDetailRowHeight = 44;
constexpr int kMaxLinesVisibleFromPasswordNote = 3;

void WriteToClipboard(const std::u16string& text) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(text);
}

std::unique_ptr<views::View> CreateIconView(
    const gfx::VectorIcon& vector_icon) {
  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icon, ui::kColorIconSecondary, kIconSize));
  return icon;
}

// Creates a view of the same height as the height of the each row in the table,
// and vertically centers the child view inside it. This is used to wrap icons
// and image buttons to ensure the icons are vertically aligned with the center
// of the first row in the text that lives inside labels in the same row even if
// the text spans multiple lines such as password notes.
std::unique_ptr<views::View> CreateWrappedView(
    std::unique_ptr<views::View> child_view) {
  auto wrapper = std::make_unique<views::BoxLayoutView>();
  wrapper->SetPreferredSize(
      gfx::Size(/*width=*/kIconSize, /*height=*/kDetailRowHeight));
  wrapper->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  wrapper->AddChildView(std::move(child_view));
  return wrapper;
}

std::unique_ptr<views::View> CreateDetailsRow(
    const gfx::VectorIcon& row_icon,
    std::unique_ptr<views::View> detail_view,
    const gfx::VectorIcon& action_icon,
    const std::u16string& action_button_tooltip_text,
    views::Button::PressedCallback action_button_callback) {
  auto row = std::make_unique<views::FlexLayoutView>();
  row->SetCollapseMargins(true);
  row->SetDefault(
      views::kMarginsKey,
      gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  row->AddChildView(CreateWrappedView(CreateIconView(row_icon)));

  detail_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  row->AddChildView(std::move(detail_view));

  std::unique_ptr<views::ImageButton> action_button =
      CreateVectorImageButtonWithNativeTheme(std::move(action_button_callback),
                                             action_icon, kIconSize);
  action_button->SetTooltipText(action_button_tooltip_text);
  row->AddChildView(CreateWrappedView(std::move(action_button)));
  return row;
}

std::unique_ptr<views::Label> CreateNoteLabel(
    const password_manager::PasswordForm& form) {
  // TODO(crbug.com/1382017): use internationalized string.
  std::u16string note_to_display = u"No note added";
  absl::optional<std::u16string> note =
      form.GetNoteWithEmptyUniqueDisplayName();
  // TODO(crbug.com/1408790): Consider adding another API to the password form
  // that returns the value directly instead of having to check whether a value
  // is set or not in all UI surfaces.
  if (note.has_value() && !note.value().empty()) {
    note_to_display = note.value();
  }

  auto note_label = std::make_unique<views::Label>(
      std::move(note_to_display), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  note_label->SetMultiLine(true);
  // TODO(crbug.com/1408790): The label should scroll when contains more lines.
  note_label->SetMaxLines(kMaxLinesVisibleFromPasswordNote);
  // TODO(crbug.com/1382017): Review string with UX and use internationalized
  // string.
  note_label->SetAccessibleName(u"Password Note");
  int line_height = views::style::GetLineHeight(note_label->GetTextContext(),
                                                note_label->GetTextStyle());
  int vertical_margin = (kDetailRowHeight - line_height) / 2;
  note_label->SetProperty(views::kMarginsKey,
                          gfx::Insets::VH(vertical_margin, 0));
  note_label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_TOP);
  note_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  return note_label;
}

}  // namespace

ManagePasswordsView::ManagePasswordsView(content::WebContents* web_contents,
                                         views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kRevampedPasswordManagementBubble));
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Title insets assume there is content (and thus have no bottom padding). Use
  // dialog insets to get the bottom margin back.
  set_title_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  // Set the right and left margins to 0 such that the `page_container_` fills
  // the whole page bubble width. Top margin is handled by the title above, and
  // remove bottom margin such that `page_container_` can assign it if needed.
  set_margins(gfx::Insets());

  page_container_ = AddChildView(
      std::make_unique<PageSwitcherView>(CreatePasswordListView()));

  if (!controller_.local_credentials().empty()) {
    // The request is cancelled when the |controller_| is destroyed.
    // |controller_| has the same lifetime as |this| and hence it's safe to use
    // base::Unretained(this).
    controller_.RequestFavicon(base::BindOnce(
        &ManagePasswordsView::OnFaviconReady, base::Unretained(this)));
  }
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetFootnoteView(CreateFooterView());
}

ManagePasswordsView::~ManagePasswordsView() = default;

PasswordBubbleControllerBase* ManagePasswordsView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* ManagePasswordsView::GetController() const {
  return &controller_;
}

ui::ImageModel ManagePasswordsView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void ManagePasswordsView::AddedToWidget() {
  // Since PasswordBubbleViewBase creates the bubble using
  // BubbleDialogDelegateView::CreateBubble() *after* the construction of the
  // ManagePasswordsView, the title view cannot be set in the constructor.
  GetBubbleFrameView()->SetTitleView(CreatePasswordListTitleView());
}

std::unique_ptr<views::View> ManagePasswordsView::CreatePasswordListTitleView()
    const {
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  auto header = std::make_unique<views::BoxLayoutView>();
  // Set the space between the icon and title similar to the default behavior in
  // BubbleFrameView::Layout().
  header->SetBetweenChildSpacing(
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).left());
  header->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon,
          layout_provider->GetDistanceMetric(
              DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE))));
  // TODO(crbug.com/1382017): refactor to use the title provided by the
  // controller instead.
  header->AddChildView(views::BubbleFrameView::CreateDefaultTitleLabel(
      u"Saved passwords for this site"));
  return header;
}

std::unique_ptr<views::View>
ManagePasswordsView::CreatePasswordDetailsTitleView() {
  DCHECK(currently_selected_password_.has_value());
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  auto header = std::make_unique<views::BoxLayoutView>();
  // Set the space between the icons and title similar to the default behavior
  // in BubbleFrameView::Layout().
  header->SetBetweenChildSpacing(
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).left());

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(
          [](ManagePasswordsView* view) {
            view->currently_selected_password_ = absl::nullopt;
            view->RecreateLayout();
          },
          base::Unretained(this)),
      vector_icons::kArrowBackIcon);
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  views::InstallCircleHighlightPathGenerator(back_button.get());
  header->AddChildView(std::move(back_button));

  std::string shown_origin = password_manager::GetShownOriginAndLinkUrl(
                                 currently_selected_password_.value())
                                 .first;
  header->AddChildView(views::BubbleFrameView::CreateDefaultTitleLabel(
      base::UTF8ToUTF16(shown_origin)));
  return header;
}

std::unique_ptr<views::View> ManagePasswordsView::CreatePasswordListView() {
  auto container_view = std::make_unique<views::BoxLayoutView>();
  container_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  for (const password_manager::PasswordForm& password_form :
       controller_.local_credentials()) {
    // TODO(crbug.com/1382017): Make sure the alignment works for different use
    // cases. (e.g. long username, federated credentials)
    container_view->AddChildView(std::make_unique<RichHoverButton>(
        base::BindRepeating(
            [](ManagePasswordsView* view,
               const password_manager::PasswordForm& password_form) {
              view->currently_selected_password_ = password_form;
              view->RecreateLayout();
            },
            base::Unretained(this), password_form),
        /*main_image_icon=*/GetFaviconImageModel(),
        /*title_text=*/GetDisplayUsername(password_form),
        /*secondary_text=*/std::u16string(),
        /*tooltip_text=*/std::u16string(),
        /*subtitle_text=*/std::u16string(),
        /*action_image_icon=*/
        ui::ImageModel::FromVectorIcon(vector_icons::kSubmenuArrowIcon,
                                       ui::kColorIcon),
        /*state_icon=*/absl::nullopt));
  }

  container_view->AddChildView(std::make_unique<views::Separator>());

  container_view->AddChildView(std::make_unique<RichHoverButton>(
      base::BindRepeating(
          [](ManagePasswordsView* view) {
            view->controller_.OnManageClicked(
                password_manager::ManagePasswordsReferrer::
                    kManagePasswordsBubble);
            view->CloseBubble();
          },
          base::Unretained(this)),
      /*main_image_icon=*/
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                     ui::kColorIcon),
      /*title_text=*/
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON),
      /*secondary_text=*/std::u16string(),
      /*tooltip_text=*/
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON),
      /*subtitle_text=*/std::u16string(),
      /*action_image_icon=*/
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                     ui::kColorIconSecondary,
                                     GetLayoutConstant(PAGE_INFO_ICON_SIZE)),
      /*state_icon=*/absl::nullopt));
  return container_view;
}

std::unique_ptr<views::View> ManagePasswordsView::CreatePasswordDetailsView()
    const {
  DCHECK(currently_selected_password_.has_value());
  auto container_view = std::make_unique<views::BoxLayoutView>();
  container_view->SetOrientation(views::BoxLayout::Orientation::kVertical);

  // TODO(crbug.com/1408790): Handle the empty username case.
  container_view->AddChildView(CreateDetailsRow(
      kAccountCircleIcon, CreateUsernameLabel(*currently_selected_password_),
      vector_icons::kContentCopyIcon,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_COPY_USERNAME),
      base::BindRepeating(&WriteToClipboard,
                          currently_selected_password_->username_value)));

  // TODO(crbug.com/1408790): Add a key icon to the password field to reveal the
  // password.
  container_view->AddChildView(CreateDetailsRow(
      kKeyIcon, CreatePasswordLabel(*currently_selected_password_),
      vector_icons::kContentCopyIcon,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_COPY_PASSWORD),
      base::BindRepeating(&WriteToClipboard,
                          currently_selected_password_->password_value)));

  // TODO(crbug.com/1408790): Use a different icon for the notes to match the
  // mocks.
  // TODO(crbug.com/1408790): Assign action to the note action button.
  // TODO(crbug.com/1408790): use internationalized string for the note action
  // button tooltip text.
  container_view->AddChildView(CreateDetailsRow(
      kAccountCircleIcon, CreateNoteLabel(*currently_selected_password_),
      vector_icons::kEditIcon, u"Edit Note", views::Button::PressedCallback()));
  return container_view;
}

std::unique_ptr<views::View> ManagePasswordsView::CreateFooterView() {
  base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
      [](ManagePasswordsView* dialog) {
        dialog->controller_.OnGooglePasswordManagerLinkClicked();
      },
      base::Unretained(this));

  switch (controller_.GetPasswordSyncState()) {
    case password_manager::SyncState::kNotSyncing:
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_BUBBLES_FOOTER_SAVING_ON_DEVICE,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE,
          open_password_manager_closure);
    case password_manager::SyncState::kSyncingNormalEncryption:
    case password_manager::SyncState::kSyncingWithCustomPassphrase:
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_BUBBLES_FOOTER_SYNCED_TO_ACCOUNT,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
          controller_.GetPrimaryAccountEmail(), open_password_manager_closure);
    case password_manager::SyncState::kAccountPasswordsActiveNormalEncryption:
      // Account store users have a special footer in the management bubble
      // since they might have a mix of synced and non-synced passwords.
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_MANAGEMENT_BUBBLE_FOOTER_ACCOUNT_STORE_USERS,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
          open_password_manager_closure);
  }
}

void ManagePasswordsView::RecreateLayout() {
  DCHECK(GetBubbleFrameView());
  if (currently_selected_password_.has_value()) {
    // TODO(crbug.com/1382017): implement authentication before navigating to
    // the details page.
    GetBubbleFrameView()->SetTitleView(CreatePasswordDetailsTitleView());
    page_container_->SwitchToPage(CreatePasswordDetailsView());
  } else {
    GetBubbleFrameView()->SetTitleView(CreatePasswordListTitleView());
    page_container_->SwitchToPage(CreatePasswordListView());
  }
  PreferredSizeChanged();
  SizeToContents();
}

void ManagePasswordsView::OnFaviconReady(const gfx::Image& favicon) {
  if (!favicon.IsEmpty()) {
    favicon_ = favicon;
    RecreateLayout();
  }
}

ui::ImageModel ManagePasswordsView::GetFaviconImageModel() const {
  // Use a globe fallback icon until the actual favicon is loaded.
  return favicon_.IsEmpty() ? ui::ImageModel::FromVectorIcon(
                                  kGlobeIcon, ui::kColorIcon, gfx::kFaviconSize)
                            : ui::ImageModel::FromImage(favicon_);
}
