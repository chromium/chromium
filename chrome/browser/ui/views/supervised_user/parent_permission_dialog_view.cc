// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/supervised_user/parent_permission_dialog_view.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extension_permissions_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace {
constexpr int kPermissionSectionPaddingTop = 20;
constexpr int kPermissionSectionPaddingBottom = 20;
constexpr int kInvalidCredentialLabelFontSizeDelta = 1;
constexpr int kInvalidCredentialLabelTopPadding = 3;

// Label that may contain empty text.
// Override is needed to configure accessibility node for an empty name.
class MaybeEmptyLabel : public views::Label {
 public:
  METADATA_HEADER(MaybeEmptyLabel);

  MaybeEmptyLabel(const std::string& text, const CustomFont& font)
      : views::Label(base::UTF8ToUTF16(text), font) {}

  MaybeEmptyLabel& operator=(const MaybeEmptyLabel&) = delete;
  MaybeEmptyLabel(const MaybeEmptyLabel&) = delete;
  ~MaybeEmptyLabel() override = default;

  // views::Label:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::Label::GetAccessibleNodeData(node_data);
    if (!GetText().empty())
      node_data->SetNameChecked(GetText());
    else
      node_data->SetNameExplicitlyEmpty();
  }
};

BEGIN_METADATA(MaybeEmptyLabel, views::Label)
END_METADATA

// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
const gfx::ImageSkia& GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  return is_app ? extensions::util::GetDefaultAppIcon()
                : extensions::util::GetDefaultExtensionIcon();
}

TestParentPermissionDialogViewObserver* test_view_observer = nullptr;

}  // namespace

// Create the parent permission input section of the dialog and
// listens for updates to its controls.
class ParentPermissionInputSection : public views::TextfieldController {
 public:
  ParentPermissionInputSection(
      ParentPermissionDialogView* main_view,
      const std::vector<std::u16string>& parent_permission_email_addresses,
      int available_width)
      : main_view_(main_view) {
    DCHECK_GT(parent_permission_email_addresses.size(), 0u);

    auto view = views::Builder<views::BoxLayoutView>()
                    .SetOrientation(views::BoxLayout::Orientation::kVertical)
                    .SetBetweenChildSpacing(
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_VERTICAL))
                    .Build();

    if (parent_permission_email_addresses.size() > 1) {
      // If there is more than one parent listed, show radio buttons.
      auto select_parent_label = std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(
              IDS_PARENT_PERMISSION_PROMPT_SELECT_PARENT_LABEL),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY);
      select_parent_label->SetHorizontalAlignment(
          gfx::HorizontalAlignment::ALIGN_LEFT);
      view->AddChildView(std::move(select_parent_label));

      // Add first parent radio button
      auto parent_0_radio_button = std::make_unique<views::RadioButton>(
          std::u16string(parent_permission_email_addresses[0]), 1 /* group */);

      // Add a subscription
      parent_0_subscription_ =
          parent_0_radio_button->AddCheckedChangedCallback(base::BindRepeating(
              [](ParentPermissionDialogView* main_view,
                 const std::u16string& parent_email) {
                main_view->SetSelectedParentPermissionEmail(parent_email);
              },
              main_view, parent_permission_email_addresses[0]));

      // Select parent 0 by default.
      parent_0_radio_button->SetChecked(true);
      view->AddChildView(std::move(parent_0_radio_button));

      // Add second parent radio button.
      auto parent_1_radio_button = std::make_unique<views::RadioButton>(
          std::u16string(parent_permission_email_addresses[1]), 1 /* group */);

      parent_1_subscription_ =
          parent_1_radio_button->AddCheckedChangedCallback(base::BindRepeating(
              [](ParentPermissionDialogView* main_view,
                 const std::u16string& parent_email) {
                main_view->SetSelectedParentPermissionEmail(parent_email);
              },
              main_view, parent_permission_email_addresses[1]));

      view->AddChildView(std::move(parent_1_radio_button));

      // Default to first parent in the response.
      main_view_->SetSelectedParentPermissionEmail(
          parent_permission_email_addresses[0]);
    } else {
      // If there is just one parent, show a label with that parent's email.
      auto parent_account_label = std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(
              IDS_PARENT_PERMISSION_PROMPT_PARENT_ACCOUNT_LABEL),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY);
      parent_account_label->SetHorizontalAlignment(
          gfx::HorizontalAlignment::ALIGN_LEFT);
      view->AddChildView(std::move(parent_account_label));

      auto parent_email_label =
          std::make_unique<views::Label>(parent_permission_email_addresses[0],
                                         views::style::CONTEXT_DIALOG_BODY_TEXT,
                                         views::style::STYLE_SECONDARY);
      parent_email_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      parent_email_label->SetMultiLine(true);
      parent_email_label->SizeToFit(available_width);
      view->AddChildView(std::move(parent_email_label));
      // Since there is only one parent, just set the output value of selected
      // parent email address here..
      main_view->SetSelectedParentPermissionEmail(
          parent_permission_email_addresses[0]);
    }

    // Add the credential input field.
    std::u16string enter_password_string = l10n_util::GetStringUTF16(
        IDS_PARENT_PERMISSION_PROMPT_ENTER_PASSWORD_LABEL);
    auto enter_password_label = std::make_unique<views::Label>(
        enter_password_string, views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_SECONDARY);
    enter_password_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    view->AddChildView(std::move(enter_password_label));

    credential_input_field_ =
        view->AddChildView(std::make_unique<views::Textfield>());
    credential_input_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    credential_input_field_->SetAccessibleName(enter_password_string);
    credential_input_field_->RequestFocus();
    credential_input_field_->set_controller(this);

    const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
    const gfx::Insets content_insets = provider->GetDialogInsetsForContentType(
        views::DialogContentType::kControl, views::DialogContentType::kControl);
    view->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        0, content_insets.left(), 0, content_insets.right())));

    // Add to main view.
    main_view->AddChildView(std::move(view));
  }

  ParentPermissionInputSection(const ParentPermissionInputSection&) = delete;
  ParentPermissionInputSection& operator=(const ParentPermissionInputSection&) =
      delete;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    main_view_->SetParentPermissionCredential(new_contents);
  }

  void ClearCredentialInputField() {
    credential_input_field_->SetText(std::u16string());
  }
  void FocusCredentialInputField() { credential_input_field_->RequestFocus(); }

 private:
  void OnParentRadioButtonSelected(ParentPermissionDialogView* main_view,
                                   const std::u16string& parent_email) {
    main_view->SetSelectedParentPermissionEmail(parent_email);
  }

  base::CallbackListSubscription parent_0_subscription_;
  base::CallbackListSubscription parent_1_subscription_;

  // The credential input field.
  raw_ptr<views::Textfield> credential_input_field_ = nullptr;

  // Owned by the parent view class, not this class.
  raw_ptr<ParentPermissionDialogView> main_view_;
};

struct ParentPermissionDialogView::Params {
  Params();
  explicit Params(const Params& params);
  ~Params();

  // The icon to be displayed. Usage depends on whether extension is set.
  gfx::ImageSkia icon;

  // The message to show. Ignored if extension is set.
  std::u16string message;

  // An optional extension whose permissions should be displayed
  raw_ptr<const extensions::Extension, DanglingUntriaged> extension = nullptr;

  // The user's profile
  raw_ptr<Profile> profile = nullptr;

  // The parent window to this window. This member may be nullptr.
  gfx::NativeWindow window = nullptr;

  // The callback to call on completion.
  ParentPermissionDialog::DoneCallback done_callback;
};

ParentPermissionDialogView::Params::Params() = default;
ParentPermissionDialogView::Params::~Params() = default;

// ParentPermissionDialogView
ParentPermissionDialogView::ParentPermissionDialogView(
    std::unique_ptr<Params> params,
    ParentPermissionDialogView::Observer* observer)
    : params_(std::move(params)), observer_(observer) {
  SetDefaultButton(ui::DIALOG_BUTTON_OK);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_PARENT_PERMISSION_PROMPT_APPROVE_BUTTON));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_PARENT_PERMISSION_PROMPT_CANCEL_BUTTON));

  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowCloseButton(true);
  SetCloseCallback(base::BindOnce(&ParentPermissionDialogView::OnDialogClose,
                                  base::Unretained(this)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  identity_manager_ = IdentityManagerFactory::GetForProfile(params_->profile);
}

ParentPermissionDialogView::~ParentPermissionDialogView() {
  // Let the observer know that this object is being destroyed.
  if (observer_)
    observer_->OnParentPermissionDialogViewDestroyed();

  // If the object is being destroyed but the callback hasn't been run, then
  // this is a failure case.
  if (params_->done_callback) {
    std::move(params_->done_callback)
        .Run(ParentPermissionDialog::Result::kParentPermissionFailed);
  }
}

void ParentPermissionDialogView::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

void ParentPermissionDialogView::SetRepromptAfterIncorrectCredential(
    bool reprompt) {
  if (reprompt_after_incorrect_credential_ == reprompt)
    return;
  reprompt_after_incorrect_credential_ = reprompt;
  OnPropertyChanged(&reprompt_after_incorrect_credential_,
                    views::kPropertyEffectsNone);
}

bool ParentPermissionDialogView::GetRepromptAfterIncorrectCredential() const {
  return reprompt_after_incorrect_credential_;
}

std::u16string ParentPermissionDialogView::GetActiveUserFirstName() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::UserManager* manager = user_manager::UserManager::Get();
  const user_manager::User* user = manager->GetActiveUser();
  return user->GetGivenName();
#else
  // TODO(https://crbug.com/1218633): Implement support for parent approved
  // extensions in LaCrOS.
  return std::u16string();
#endif
}

void ParentPermissionDialogView::AddedToWidget() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  constexpr int icon_size = extension_misc::EXTENSION_ICON_SMALL;
  auto message_container =
      views::Builder<views::TableLayoutView>()
          .AddColumn(views::LayoutAlignment::kCenter,
                     views::LayoutAlignment::kStart,
                     views::TableLayout::kFixedSize,
                     views::TableLayout::ColumnSize::kFixed, icon_size, 0)
          // Equalize padding on the left and the right of the icon.
          .AddPaddingColumn(
              views::TableLayout::kFixedSize,
              provider->GetInsetsMetric(views::INSETS_DIALOG).left())
          // Set a resize weight so that the message label will be expanded to
          // the available width.
          .AddColumn(views::LayoutAlignment::kStretch,
                     views::LayoutAlignment::kStart, 1.0,
                     views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
          .AddRows(1, views::TableLayout::kFixedSize, 0);

  // Scale down to icon size, but allow smaller icons (don't scale up).
  if (!params_->icon.isNull()) {
    const gfx::ImageSkia& image = params_->icon;
    gfx::Size size(image.width(), image.height());
    auto icon = std::make_unique<views::ImageView>();
    size.SetToMin(gfx::Size(icon_size, icon_size));
    message_container.AddChild(
        views::Builder<views::ImageView>().SetImageSize(size).SetImage(image));
  } else {
    // Add an empty view if there is no icon. This is required to ensure the
    // the label below still lands in the correct TableLayout column.
    message_container.AddChild(views::Builder<views::View>());
  }

  DCHECK(!params_->message.empty());
  message_container.AddChild(
      views::Builder<views::Label>(
          views::BubbleFrameView::CreateDefaultTitleLabel(params_->message))
          // Setting the message's preferred size to 0 ensures it won't
          // influence the overall size of the dialog. It will be expanded by
          // TableLayout.
          .SetPreferredSize(gfx::Size(0, 0)));

  GetBubbleFrameView()->SetTitleView(std::move(message_container).Build());
}

void ParentPermissionDialogView::OnThemeChanged() {
  views::DialogDelegateView::OnThemeChanged();
  invalid_credential_label_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorAlertHighSeverity));
}

void ParentPermissionDialogView::OnDialogClose() {
  // If the dialog is closed without the user clicking "approve" consider this
  // as ParentPermissionCanceled to avoid showing an error message. If the
  // user clicked "accept", then that async process will send the result, or if
  // that doesn't complete, eventually the destructor will send a failure
  // result.
  if (!is_approve_clicked_) {
    SendResultOnce(ParentPermissionDialog::Result::kParentPermissionCanceled);
  }
}

bool ParentPermissionDialogView::Cancel() {
  SendResultOnce(ParentPermissionDialog::Result::kParentPermissionCanceled);
  return true;
}

bool ParentPermissionDialogView::Accept() {
  is_approve_clicked_ = true;
  // Disable the dialog temporarily while we validate the parent's credentials,
  // which can take some time because it involves a series of async network
  // requests.
  SetEnabled(false);
  // Clear out the invalid credential label, so that it disappears/reappears to
  // the user to emphasize that the password check happened again.
  invalid_credential_label_->SetText(std::u16string());
  std::string parent_obfuscated_gaia_id =
      GetParentObfuscatedGaiaID(selected_parent_permission_email_);
  std::string parent_credential =
      base::UTF16ToUTF8(parent_permission_credential_);
  StartReauthAccessTokenFetch(parent_obfuscated_gaia_id, parent_credential);

  return false;
}

std::u16string ParentPermissionDialogView::GetAccessibleWindowTitle() const {
  return params_->message;
}

void ParentPermissionDialogView::CreateContents() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets content_insets = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl);
  const int content_width = GetPreferredSize().width() - content_insets.width();
  set_margins(
      gfx::Insets::TLBR(content_insets.top(), 0, content_insets.bottom(), 0));

  // Extension-specific views.
  if (params_->extension && !prompt_permissions_.permissions.empty()) {
    auto install_permissions_section_container =
        std::make_unique<views::View>();
    install_permissions_section_container->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(
            kPermissionSectionPaddingTop, content_insets.left(),
            kPermissionSectionPaddingBottom, content_insets.right())));
    install_permissions_section_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical, gfx::Insets(),
            provider->GetDistanceMetric(
                views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    // Set up the permissions header string.
    // Shouldn't be asking for permissions for theme installs.
    DCHECK(!params_->extension->is_theme());
    std::u16string extension_type;
    if (params_->extension->is_extension()) {
      extension_type = l10n_util::GetStringUTF16(
          IDS_PARENT_PERMISSION_PROMPT_EXTENSION_TYPE_EXTENSION);
    } else if (params_->extension->is_app()) {
      extension_type = l10n_util::GetStringUTF16(
          IDS_PARENT_PERMISSION_PROMPT_EXTENSION_TYPE_APP);
    }
    std::u16string permission_header_label = l10n_util::GetStringFUTF16(
        IDS_PARENT_PERMISSION_PROMPT_CHILD_WANTS_TO_INSTALL_LABEL,
        GetActiveUserFirstName(), extension_type);

    views::Label* permissions_header = new views::Label(
        permission_header_label, views::style::CONTEXT_DIALOG_BODY_TEXT);
    permissions_header->SetMultiLine(true);
    permissions_header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    permissions_header->SizeToFit(content_width);
    permissions_header->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        0, content_insets.left(), 0, content_insets.right())));

    // Add this outside the scrolling section, so it can't be obscured by
    // scrolling.
    AddChildView(permissions_header);

    // Create permissions view.
    auto permissions_view =
        std::make_unique<ExtensionPermissionsView>(content_width);
    permissions_view->AddPermissions(prompt_permissions_);

    // Add to the section container, so the permissions can scroll, since they
    // can be arbitrarily long.
    install_permissions_section_container->AddChildView(
        std::move(permissions_view));

    // Add section container to an enclosing scroll view.
    auto scroll_view = std::make_unique<views::ScrollView>();
    scroll_view->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    scroll_view->SetContents(std::move(install_permissions_section_container));
    scroll_view->ClipHeightTo(
        0, provider->GetDistanceMetric(
               views::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT));
    AddChildView(std::move(scroll_view));
  }

  // Create the parent permission section, which contains controls
  // for parent selection and password entry.
  parent_permission_input_section_ =
      std::make_unique<ParentPermissionInputSection>(
          this, parent_permission_email_addresses_, content_width);

  // Add the invalid credential label, which is initially empty,
  // and hence invisible.  It will be updated if the user enters
  // an incorrect password.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::Label::CustomFont font = {
      rb.GetFontListWithDelta(kInvalidCredentialLabelFontSizeDelta)};
  auto invalid_credential_label = std::make_unique<MaybeEmptyLabel>("", font);

  invalid_credential_label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kInvalidCredentialLabelTopPadding,
                        content_insets.left(), 0, content_insets.right())));
  invalid_credential_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  invalid_credential_label->SetMultiLine(true);
  invalid_credential_label->SizeToFit(content_width);

  // Cache the pointer so we we can update the invalid credential label when we
  // get an incorrect password.
  invalid_credential_label_ = AddChildView(std::move(invalid_credential_label));
}

void ParentPermissionDialogView::ShowDialog() {
  if (is_showing_)
    return;

  is_showing_ = true;
  LoadParentEmailAddresses();

  supervised_user_metrics_recorder_.RecordParentPermissionDialogUmaMetrics(
      SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
          kOpened);

  if (params_->extension)
    InitializeExtensionData(params_->extension.get());
  else
    ShowDialogInternal();
}

void ParentPermissionDialogView::CloseDialog() {
  CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void ParentPermissionDialogView::RemoveObserver() {
  observer_ = nullptr;
}

void ParentPermissionDialogView::SetSelectedParentPermissionEmail(
    const std::u16string& email_address) {
  if (selected_parent_permission_email_ == email_address)
    return;
  selected_parent_permission_email_ = email_address;
  OnPropertyChanged(&selected_parent_permission_email_,
                    views::kPropertyEffectsNone);
}

std::u16string ParentPermissionDialogView::GetSelectedParentPermissionEmail()
    const {
  return selected_parent_permission_email_;
}

void ParentPermissionDialogView::SetParentPermissionCredential(
    const std::u16string& credential) {
  if (parent_permission_credential_ == credential)
    return;
  parent_permission_credential_ = credential;
  OnPropertyChanged(&parent_permission_credential_,
                    views::kPropertyEffectsNone);
}

std::u16string ParentPermissionDialogView::GetParentPermissionCredential()
    const {
  return parent_permission_credential_;
}

bool ParentPermissionDialogView::GetInvalidCredentialReceived() const {
  return invalid_credential_received_;
}

void ParentPermissionDialogView::ShowDialogInternal() {
  // The contents have to be created here, instead of during construction
  // because they can potentially rely on the side effects of loading info
  // from an extension.
  CreateContents();
  views::Widget* widget =
      params_->window
          ? constrained_window::CreateBrowserModalDialogViews(this,
                                                              params_->window)
          : views::DialogDelegate::CreateDialogWidget(this, nullptr, nullptr);
  widget->Show();

  if (test_view_observer)
    test_view_observer->OnTestParentPermissionDialogViewCreated(this);
}

void ParentPermissionDialogView::LoadParentEmailAddresses() {
  // Get the parents' email addresses.  There can be a max of 2 parent email
  // addresses, the primary and the secondary.
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(params_->profile);

  std::u16string primary_parent_email =
      base::UTF8ToUTF16(service->GetCustodianEmailAddress());
  if (!primary_parent_email.empty())
    parent_permission_email_addresses_.push_back(primary_parent_email);

  std::u16string secondary_parent_email =
      base::UTF8ToUTF16(service->GetSecondCustodianEmailAddress());
  if (!secondary_parent_email.empty())
    parent_permission_email_addresses_.push_back(secondary_parent_email);

  if (parent_permission_email_addresses_.empty()) {
    supervised_user_metrics_recorder_.RecordParentPermissionDialogUmaMetrics(
        SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
            kNoParentError);
    SendResultOnce(ParentPermissionDialog::Result::kParentPermissionFailed);
  }
}

void ParentPermissionDialogView::OnExtensionIconLoaded(
    const gfx::Image& image) {
  // The order of preference for the icon to use is:
  //  1. Icon loaded from extension, if not empty.
  //  2. Icon passed in params, if not empty.
  //  3. Default Icon.
  if (!image.IsEmpty()) {
    // Use the image that was loaded from the extension if it's not empty
    params_->icon = *image.ToImageSkia();
  } else if (params_->icon.isNull()) {
    // If icon is empty, use a default icon.:
    params_->icon =
        GetDefaultIconBitmapForMaxScaleFactor(params_->extension->is_app());
  }

  ShowDialogInternal();
}

void ParentPermissionDialogView::LoadExtensionIcon() {
  DCHECK(params_->extension);

  // Load the image asynchronously. The response will be sent to
  // OnExtensionIconLoaded.
  extensions::ImageLoader* loader =
      extensions::ImageLoader::Get(params_->profile);
  loader->LoadImageAtEveryScaleFactorAsync(
      params_->extension,
      gfx::Size(extension_misc::EXTENSION_ICON_LARGE,
                extension_misc::EXTENSION_ICON_LARGE),
      base::BindOnce(&ParentPermissionDialogView::OnExtensionIconLoaded,
                     weak_factory_.GetWeakPtr()));
}

void ParentPermissionDialogView::CloseWithReason(
    views::Widget::ClosedReason reason) {
  views::Widget* widget = GetWidget();
  if (widget) {
    widget->CloseWithReason(reason);
  } else {
    //  The widget has disappeared, so delete this view.
    delete this;
  }
}

std::string ParentPermissionDialogView::GetParentObfuscatedGaiaID(
    const std::u16string& parent_email) const {
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(params_->profile);

  if (service->GetCustodianEmailAddress() == base::UTF16ToUTF8(parent_email))
    return service->GetCustodianObfuscatedGaiaId();

  if (service->GetSecondCustodianEmailAddress() ==
      base::UTF16ToUTF8(parent_email)) {
    return service->GetSecondCustodianObfuscatedGaiaId();
  }

  NOTREACHED()
      << "Tried to get obfuscated gaia id for a non-custodian email address";
  return std::string();
}

void ParentPermissionDialogView::StartReauthAccessTokenFetch(
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential) {
  // The first step of Reauth is to fetch an OAuth2 access token for the
  // Reauth API scope.
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAccountsReauthOAuth2Scope);
  oauth2_access_token_fetcher_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync),
          "chrome_webstore_private_api", scopes,
          base::BindOnce(
              &ParentPermissionDialogView::OnAccessTokenFetchComplete,
              weak_factory_.GetWeakPtr(), parent_obfuscated_gaia_id,
              parent_credential),
          signin::AccessTokenFetcher::Mode::kImmediate);
}

void ParentPermissionDialogView::OnAccessTokenFetchComplete(
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  oauth2_access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    SendResultOnce(ParentPermissionDialog::Result::kParentPermissionFailed);
    CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    return;
  }

  // Now that we have the OAuth2 access token, we use it when we attempt
  // to fetch the ReauthProof token (RAPT) for the parent.
  StartParentReauthProofTokenFetch(
      access_token_info.token, parent_obfuscated_gaia_id, parent_credential);
}

void ParentPermissionDialogView::StartParentReauthProofTokenFetch(
    const std::string& child_access_token,
    const std::string& parent_obfuscated_gaia_id,
    const std::string& credential) {
  reauth_token_fetcher_ = std::make_unique<GaiaAuthFetcher>(
      this, gaia::GaiaSource::kChromeOS,
      params_->profile->GetURLLoaderFactory());
  reauth_token_fetcher_->StartCreateReAuthProofTokenForParent(
      child_access_token, parent_obfuscated_gaia_id, credential);
}

void ParentPermissionDialogView::SendResultOnce(
    ParentPermissionDialog::Result result) {
  if (!params_->done_callback)
    return;
  // Record UMA metrics.
  switch (result) {
    case ParentPermissionDialog::Result::kParentPermissionReceived:
      supervised_user_metrics_recorder_.RecordParentPermissionDialogUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
              kParentApproved);
      break;
    case ParentPermissionDialog::Result::kParentPermissionCanceled:
      supervised_user_metrics_recorder_.RecordParentPermissionDialogUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
              kParentCanceled);
      break;
    case ParentPermissionDialog::Result::kParentPermissionFailed:
      supervised_user_metrics_recorder_.RecordParentPermissionDialogUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
              kFailed);
      break;
  }
  std::move(params_->done_callback).Run(result);
}

void ParentPermissionDialogView::OnReAuthProofTokenSuccess(
    const std::string& reauth_proof_token) {
  SendResultOnce(ParentPermissionDialog::Result::kParentPermissionReceived);
  CloseWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);
}

void ParentPermissionDialogView::OnReAuthProofTokenFailure(
    const GaiaAuthConsumer::ReAuthProofTokenStatus error) {
  reauth_token_fetcher_.reset();
  if (error == GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant) {
    // If invalid password was entered, and the dialog is configured to
    // re-prompt  show the dialog again with the invalid password error message.
    // prompt again, this time with a password error message.
    invalid_credential_received_ = true;
    if (reprompt_after_incorrect_credential_) {
      SetEnabled(true);
      parent_permission_input_section_->ClearCredentialInputField();
      parent_permission_input_section_->FocusCredentialInputField();
      invalid_credential_label_->SetText(l10n_util::GetStringUTF16(
          IDS_PARENT_PERMISSION_PROMPT_PASSWORD_INCORRECT_LABEL));
      invalid_credential_label_->NotifyAccessibilityEvent(
          ax::mojom::Event::kAlert, true);
      return;
    }
  }
  SendResultOnce(ParentPermissionDialog::Result::kParentPermissionFailed);
  CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void ParentPermissionDialogView::InitializeExtensionData(
    scoped_refptr<const extensions::Extension> extension) {
  DCHECK(extension);

  // Load Permissions.
  std::unique_ptr<const extensions::PermissionSet> permissions_to_display =
      extensions::util::GetInstallPromptPermissionSetForExtension(
          extension.get(), params_->profile,
          // Matches behavior of regular extension install prompt because this
          // prompt is never used for delegated permissions, which the only
          // time optional permissions are shown.
          false /* include_optional_permissions */
      );
  extensions::Manifest::Type type = extension->GetType();
  prompt_permissions_.LoadFromPermissionSet(permissions_to_display.get(), type);

  // Create the dialog's message using the extension's name.
  params_->message = l10n_util::GetStringFUTF16(
      IDS_PARENT_PERMISSION_PROMPT_GO_GET_A_PARENT_FOR_EXTENSION_LABEL,
      base::UTF8ToUTF16(extension->name()));

  LoadExtensionIcon();
}

BEGIN_METADATA(ParentPermissionDialogView, views::DialogDelegateView)
ADD_PROPERTY_METADATA(std::u16string, SelectedParentPermissionEmail)
ADD_PROPERTY_METADATA(std::u16string, ParentPermissionCredential)
ADD_READONLY_PROPERTY_METADATA(bool, InvalidCredentialReceived)
ADD_PROPERTY_METADATA(bool, RepromptAfterIncorrectCredential)
ADD_READONLY_PROPERTY_METADATA(std::u16string, ActiveUserFirstName)
END_METADATA

class ParentPermissionDialogImpl : public ParentPermissionDialog,
                                   public ParentPermissionDialogView::Observer {
 public:
  // Constructor for a generic ParentPermissionDialogImpl
  ParentPermissionDialogImpl(
      std::unique_ptr<ParentPermissionDialogView::Params> params);

  ~ParentPermissionDialogImpl() override;

  // ParentPermissionDialog
  void ShowDialog() override;

  // ParentPermissionDialogView::Observer
  void OnParentPermissionDialogViewDestroyed() override;

 private:
  raw_ptr<ParentPermissionDialogView> view_ = nullptr;
};

ParentPermissionDialogImpl::ParentPermissionDialogImpl(
    std::unique_ptr<ParentPermissionDialogView::Params> params)
    : view_(new ParentPermissionDialogView(std::move(params), this)) {}

void ParentPermissionDialogImpl::ShowDialog() {
  // Ownership of dialog_view is passed to the views system when the dialog is
  // shown here.  We check for the validity of view_ because in theory it could
  // disappear from beneath this object before ShowDialog() is called.
  if (view_)
    view_->ShowDialog();
}

ParentPermissionDialogImpl::~ParentPermissionDialogImpl() {
  // We check for the validity of view_ because in theory it could
  // disappear from beneath this object before ShowDialog() is called.
  if (view_) {
    // Important to remove the observer here, so that we don't try to use it in
    // the destructor to inform the ParentPermissionDialog, which would cause a
    // use-after-free.
    view_->RemoveObserver();
    view_->CloseDialog();
  }
}

void ParentPermissionDialogImpl::OnParentPermissionDialogViewDestroyed() {
  // The underlying ParentPermissionDialogView has been destroyed.
  view_ = nullptr;
}

// static
std::unique_ptr<ParentPermissionDialog>
ParentPermissionDialog::CreateParentPermissionDialog(
    Profile* profile,
    gfx::NativeWindow window,
    const gfx::ImageSkia& icon,
    const std::u16string& message,
    ParentPermissionDialog::DoneCallback done_callback) {
  auto params = std::make_unique<ParentPermissionDialogView::Params>();
  params->message = message;
  params->icon = icon;
  params->profile = profile;
  params->window = window;
  params->done_callback = std::move(done_callback);

  return std::make_unique<ParentPermissionDialogImpl>(std::move(params));
}

// static
std::unique_ptr<ParentPermissionDialog>
ParentPermissionDialog::CreateParentPermissionDialogForExtension(
    Profile* profile,
    gfx::NativeWindow window,
    const gfx::ImageSkia& icon,
    const extensions::Extension* extension,
    ParentPermissionDialog::DoneCallback done_callback) {
  auto params = std::make_unique<ParentPermissionDialogView::Params>();
  params->extension = extension;
  params->icon = icon;
  params->profile = profile;
  params->window = window;
  params->done_callback = std::move(done_callback);

  return std::make_unique<ParentPermissionDialogImpl>(std::move(params));
}

TestParentPermissionDialogViewObserver::TestParentPermissionDialogViewObserver(
    TestParentPermissionDialogViewObserver* observer) {
  DCHECK(!test_view_observer);
  test_view_observer = observer;
}

TestParentPermissionDialogViewObserver::
    ~TestParentPermissionDialogViewObserver() {
  test_view_observer = nullptr;
}
