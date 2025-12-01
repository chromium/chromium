// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_input_protector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extension_permissions_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

// Time delay before the install button is enabled after initial display.
int g_install_delay_in_ms = 500;

// Returns the accessible name for the ratings view, which will convey the
// information properly (i.e., "Rated 4.2 stars by 379 reviews" rather than
// "image image...379").
std::u16string GetRatingAccessibleName(double rating, int rating_count) {
  if (rating_count == 0) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSION_PROMPT_NO_RATINGS_ACCESSIBLE_TEXT);
  }
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_RATING_ACCESSIBLE_TEXT),
      rating, rating_count);
}

void ShowExtensionInstallDialogImpl(
    std::unique_ptr<ExtensionInstallPromptShowParams> show_params,
    ExtensionInstallPrompt::DoneCallback done_callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the dialog has to be parented to WebContents, force activate the
  // contents. See crbug.com/40059470.
  content::WebContents* web_contents = show_params->GetParentWebContents();
  Browser* browser =
      web_contents ? chrome::FindBrowserWithTab(web_contents) : nullptr;

  if (browser &&
      browser->tab_strip_model()->GetActiveWebContents() != web_contents) {
    browser->ActivateContents(web_contents);
  }

  gfx::NativeWindow parent_window = show_params->GetParentWindow();
  auto dialog = std::make_unique<ExtensionInstallDialogView>(
      std::move(show_params), std::move(done_callback), std::move(prompt));
  constrained_window::CreateBrowserModalDialogViews(std::move(dialog),
                                                    parent_window)
      ->Show();
}

}  // namespace

// A custom view for the justification section of the extension info. It
// contains a text field into which users can enter their justification for
// requesting an extension.
class ExtensionJustificationView : public views::BoxLayoutView {
  METADATA_HEADER(ExtensionJustificationView, views::View)

 public:
  explicit ExtensionJustificationView(views::TextfieldController* controller) {
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetBetweenChildSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_RELATED_CONTROL_VERTICAL));

    justification_field_label_ = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_EXTENSION_REQUEST_JUSTIFICATION),
        views::style::CONTEXT_DIALOG_BODY_TEXT));
    justification_field_label_->SetMultiLine(true);
    justification_field_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    justification_field_ = AddChildView(std::make_unique<views::Textarea>());
    justification_field_->SetPreferredSize(gfx::Size(0, 60));
    justification_field_->SetPlaceholderText(l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_EXTENSION_REQUEST_JUSTIFICATION_PLACEHOLDER));
    justification_field_->set_controller(controller);

    justification_text_length_ = AddChildView(std::make_unique<views::Label>());
    justification_text_length_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    justification_text_length_->SetText(l10n_util::GetStringFUTF16(
        IDS_ENTERPRISE_EXTENSION_REQUEST_JUSTIFICATION_LENGTH_LIMIT,
        base::NumberToString16(0),
        base::NumberToString16(kMaxJustificationTextLength)));
  }
  ExtensionJustificationView(const ExtensionJustificationView&) = delete;
  ExtensionJustificationView& operator=(const ExtensionJustificationView&) =
      delete;
  ~ExtensionJustificationView() override = default;

  // Get the text currently present in the justification text field.
  std::u16string_view GetJustificationText() {
    DCHECK(justification_field_);
    return justification_field_->GetText();
  }

  void SetJustificationTextForTesting(const std::u16string& new_text) {
    DCHECK(justification_field_);
    // Resets the text field to an empty string so that InsertOrReplaceText()
    // below does not append to the already entered text. Does not trigger
    // UpdateAfterChange().
    justification_field_->SetText(std::u16string());
    // Triggers UpdateAfterChange() to update the state of DialogButton::OK.
    justification_field_->InsertOrReplaceText(new_text);
  }

  bool IsJustificationLengthWithinLimit() {
    return justification_field_->GetText().length() <=
           kMaxJustificationTextLength;
  }

  void UpdateLengthLabel() {
    justification_text_length_->SetText(l10n_util::GetStringFUTF16(
        IDS_ENTERPRISE_EXTENSION_REQUEST_JUSTIFICATION_LENGTH_LIMIT,
        base::NumberToString16(justification_field_->GetText().length()),
        base::NumberToString16(kMaxJustificationTextLength)));

    // The original color is not stored because the theme may change while the
    // dialog is visible. To get around this, another label is used as the color
    // reference.
    if (IsJustificationLengthWithinLimit()) {
      justification_text_length_->SetEnabledColor(
          justification_field_label_->GetEnabledColor());
    } else {
      justification_text_length_->SetEnabledColor(ui::kColorAlertHighSeverity);
    }
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

 private:
  const size_t kMaxJustificationTextLength = 280;

  raw_ptr<views::Label> justification_field_label_;
  raw_ptr<views::Textfield> justification_field_;
  raw_ptr<views::Label> justification_text_length_;
};

BEGIN_VIEW_BUILDER(/* No Export */,
                   ExtensionJustificationView,
                   views::BoxLayoutView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* No Export */, ExtensionJustificationView)

BEGIN_METADATA(, ExtensionJustificationView)
END_METADATA

// TODO(crbug.com/355018927): Remove this when we implement in views::Label.
class TitleLabelWrapper : public views::View {
  METADATA_HEADER(TitleLabelWrapper, views::View)

 public:
  explicit TitleLabelWrapper(std::unique_ptr<views::View> title) {
    SetUseDefaultFillLayout(true);
    title_ = AddChildView(std::move(title));
  }

 private:
  // View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size preferred_size = title_->GetPreferredSize(available_size);
    if (!available_size.width().is_bounded()) {
      preferred_size.set_width(title_->GetMinimumSize().width());
    }
    return preferred_size;
  }

  raw_ptr<views::View> title_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* No Export */, TitleLabelWrapper, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* No Export */, TitleLabelWrapper)

BEGIN_METADATA(TitleLabelWrapper)
END_METADATA

ExtensionInstallDialogView::ExtensionInstallDialogView(
    std::unique_ptr<ExtensionInstallPromptShowParams> show_params,
    ExtensionInstallPrompt::DoneCallback done_callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt)
    : profile_(show_params->profile()),
      show_params_(std::move(show_params)),
      done_callback_(std::move(done_callback)),
      prompt_(std::move(prompt)),
      scroll_view_(nullptr),
      install_button_enabled_(false),
      grant_permissions_checkbox_(nullptr) {
  DCHECK(prompt_->extension());

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile_);
  extension_registry_observation_.Observe(extension_registry);

  int buttons = prompt_->GetDialogButtons();
  DCHECK(buttons & static_cast<int>(ui::mojom::DialogButton::kCancel));

  int default_button = static_cast<int>(ui::mojom::DialogButton::kCancel);

  // If the prompt is related to requesting an extension, set the default button
  // to OK.
  if (prompt_->type() ==
      ExtensionInstallPrompt::PromptType::EXTENSION_REQUEST_PROMPT) {
    default_button = static_cast<int>(ui::mojom::DialogButton::kOk);
  }

  // When we require parent permission next, we
  // set the default button to OK.
  if (prompt_->requires_parent_permission()) {
    default_button = static_cast<int>(ui::mojom::DialogButton::kOk);
  }

  SetModalType(ui::mojom::ModalType::kWindow);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetDefaultButton(default_button);
  SetButtons(buttons);
  SetAcceptCallback(base::BindOnce(
      &ExtensionInstallDialogView::OnDialogAccepted, base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      &ExtensionInstallDialogView::OnDialogCanceled, base::Unretained(this)));
  set_draggable(true);
  if (prompt_->has_webstore_data()) {
    auto store_link = std::make_unique<views::Link>(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_STORE_LINK));
    store_link->SetCallback(base::BindRepeating(
        &ExtensionInstallDialogView::LinkClicked, base::Unretained(this)));
    SetExtraView(std::move(store_link));
  } else if (prompt_->ShouldWithheldPermissionsOnDialogAccept()) {
    grant_permissions_checkbox_ = SetExtraView(
        std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_GRANT_PERMISSIONS_CHECKBOX)));
  }

  SetButtonLabel(ui::mojom::DialogButton::kOk, prompt_->GetAcceptButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 prompt_->GetAbortButtonLabel());
  set_close_on_deactivate(false);
  SetShowCloseButton(false);
  CreateContents();
}

ExtensionInstallDialogView::~ExtensionInstallDialogView() {
  if (done_callback_) {
    OnDialogCanceled();
  }
}

ExtensionInstallPromptShowParams*
ExtensionInstallDialogView::GetShowParamsForTesting() {
  return show_params_.get();
}

void ExtensionInstallDialogView::ClickLinkForTesting() {
  LinkClicked();
}

void ExtensionInstallDialogView::SetInstallButtonDelayForTesting(
    int delay_in_ms) {
  g_install_delay_in_ms = delay_in_ms;
}

bool ExtensionInstallDialogView::IsJustificationFieldVisibleForTesting() {
  return justification_view_ != nullptr;
}

void ExtensionInstallDialogView::SetJustificationTextForTesting(
    const std::u16string& new_text) {
  DCHECK(justification_view_ != nullptr);
  justification_view_->SetJustificationTextForTesting(new_text);  // IN-TEST
}

bool ExtensionInstallDialogView::
    ShouldIgnoreButtonPressedEventHandlingForTesting(
        View* button,
        const ui::Event& event) const {
  return ShouldIgnoreButtonPressedEventHandling(button, event);
}

bool ExtensionInstallDialogView::
    ShouldAllowKeyEventsDuringInputProtectionForTesting() const {
  return ShouldAllowKeyEventsDuringInputProtection();
}

void ExtensionInstallDialogView::ResizeWidget() {
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

void ExtensionInstallDialogView::VisibilityChanged(views::View* starting_from,
                                                   bool is_visible) {
  // VisibilityChanged() might spuriously fire more than once on some platforms,
  // for example, when Widget::Show() and Widget::Activate() are called
  // sequentially. Timers should be started only at the first notification of
  // visibility change.
  if (is_visible && !install_result_timer_) {
    install_result_timer_ = base::ElapsedTimer();

    if (!install_button_enabled_) {
      // This base::Unretained is safe because the task is owned by the timer,
      // which is in turn owned by this object.
      enable_install_timer_.Start(
          FROM_HERE, base::Milliseconds(g_install_delay_in_ms),
          base::BindOnce(&ExtensionInstallDialogView::EnableInstallButton,
                         base::Unretained(this)));
    }
  }
}

void ExtensionInstallDialogView::AddedToWidget() {
  auto title_container = CreateTitleContainer();
  GetBubbleFrameView()->SetTitleView(std::move(title_container));

  picture_in_picture_input_protector_ =
      std::make_unique<PictureInPictureInputProtector>(
          GetWidget()->widget_delegate()->AsDialogDelegate());
}

void ExtensionInstallDialogView::OnDialogCanceled() {
  DCHECK(done_callback_);

  // The dialog will be closed, so stop observing for any extension changes
  // that could potentially crop up during that process (like the extension
  // being uninstalled).
  extension_registry_observation_.Reset();

  prompt_->OnDialogCanceled();
  std::move(done_callback_)
      .Run(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::USER_CANCELED));
}

void ExtensionInstallDialogView::OnDialogAccepted() {
  DCHECK(done_callback_);

  // The dialog will be closed, so stop observing for any extension changes
  // that could potentially crop up during that process (like the extension
  // being uninstalled).
  extension_registry_observation_.Reset();

  bool expect_justification =
      prompt_->type() ==
      ExtensionInstallPrompt::PromptType::EXTENSION_REQUEST_PROMPT;
  DCHECK(expect_justification == !!justification_view_);

  prompt_->OnDialogAccepted();

  // Permissions are withheld at installation when the prompt specifies it and
  // `grant_permissions_checkbox_` wasn't selected.
  auto result =
      (prompt_->ShouldWithheldPermissionsOnDialogAccept() &&
       grant_permissions_checkbox_ &&
       !grant_permissions_checkbox_->GetChecked())
          ? ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS
          : ExtensionInstallPrompt::Result::ACCEPTED;

  std::move(done_callback_)
      .Run(ExtensionInstallPrompt::DoneCallbackPayload(
          result,
          justification_view_
              ? base::UTF16ToUTF8(justification_view_->GetJustificationText())
              : std::string()));
}

bool ExtensionInstallDialogView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  if (button == ui::mojom::DialogButton::kOk) {
    return install_button_enabled_ && request_button_enabled_;
  }
  return true;
}

std::u16string ExtensionInstallDialogView::GetAccessibleWindowTitle() const {
  return prompt_->GetDialogTitle();
}

bool ExtensionInstallDialogView::ShouldIgnoreButtonPressedEventHandling(
    View* button,
    const ui::Event& event) const {
  // Ignore button pressed events whenever we are occluded by a
  // Picture-in-Picture window.
  return picture_in_picture_input_protector_ &&
         picture_in_picture_input_protector_->OccludedByPictureInPicture();
}

bool ExtensionInstallDialogView::ShouldAllowKeyEventsDuringInputProtection()
    const {
  return false;
}

void ExtensionInstallDialogView::CloseDialog() {
  GetWidget()->Close();
}

void ExtensionInstallDialogView::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  // Extra checks for https://crbug.com/1259043.
  // TODO(devlin): Remove these when we've validated there's no longer a crash.
  CHECK(extension);
  CHECK(prompt_);
  CHECK(prompt_->extension());
  // Close the dialog if the extension is uninstalled.
  if (extension->id() != prompt_->extension()->id()) {
    return;
  }
  CloseDialog();
}

void ExtensionInstallDialogView::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile_);
  DCHECK_EQ(extension_registry, registry);
  DCHECK(extension_registry_observation_.IsObservingSource(extension_registry));
  extension_registry_observation_.Reset();
  CloseDialog();
}

void ExtensionInstallDialogView::LinkClicked() {
  GURL store_url(extension_urls::GetWebstoreItemDetailURLPrefix() +
                 prompt_->extension()->id());
  OpenURLParams params(store_url, Referrer(),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB,
                       ui::PAGE_TRANSITION_LINK, false);

  DCHECK(show_params_);
  if (show_params_->GetParentWebContents()) {
    show_params_->GetParentWebContents()->OpenURL(
        params, /*navigation_handle_callback=*/{});
  } else {
    chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
    displayer.browser()->OpenURL(params, /*navigation_handle_callback=*/{});
  }
  CloseDialog();
}

void ExtensionInstallDialogView::CreateContents() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  bool has_permissions = prompt_->GetPermissionCount() > 0;
  bool requires_justification =
      prompt_->type() ==
      ExtensionInstallPrompt::PromptType::EXTENSION_REQUEST_PROMPT;

  if (!has_permissions && !requires_justification) {
    // Use a smaller margin between the title area and buttons, since there
    // isn't any content.
    set_margins(
        gfx::Insets::TLBR(ChromeLayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                          0, 0, 0));
    return;
  }

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets content_insets = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl);
  set_margins(
      gfx::Insets::TLBR(content_insets.top(), 0, content_insets.bottom(), 0));

  std::unique_ptr<views::ScrollView> scroll_view =
      CreateExtensionInfoContainer(has_permissions, requires_justification);
  scroll_view_ = scroll_view.get();
  AddChildView(std::move(scroll_view));
}

std::unique_ptr<views::ScrollView>
ExtensionInstallDialogView::CreateExtensionInfoContainer(
    bool has_permissions,
    bool requires_justification) {
  CHECK(has_permissions || requires_justification);
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets content_insets = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl);

  auto extension_info_container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetInsideBorderInsets(gfx::Insets::TLBR(0, content_insets.left(), 0,
                                                   content_insets.right()))
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  if (has_permissions) {
    extension_info_container.AddChild(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetBetweenChildSpacing(provider->GetDistanceMetric(
                views::DISTANCE_RELATED_CONTROL_VERTICAL))
            .AddChildren(
                // Permissions header.
                views::Builder<views::Label>()
                    .SetText(prompt_->GetPermissionsHeading())
                    .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                    .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                    .SetMultiLine(true),
                // Permissions content.
                views::Builder<ExtensionPermissionsView>(
                    std::make_unique<ExtensionPermissionsView>(
                        prompt_->GetPermissions()))));
  }

  if (requires_justification) {
    auto justification_view =
        std::make_unique<ExtensionJustificationView>(this);
    justification_view_ = justification_view.get();
    extension_info_container.AddChild(
        views::Builder<ExtensionJustificationView>(
            std::move(justification_view)));
  }

  return views::Builder<views::ScrollView>()
      .SetHorizontalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled)
      .ClipHeightTo(0, provider->GetDistanceMetric(
                           views::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT))
      .SetContents(extension_info_container)
      .Build();
}

std::unique_ptr<views::TableLayoutView>
ExtensionInstallDialogView::CreateTitleContainer() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  constexpr int icon_size = extension_misc::EXTENSION_ICON_SMALL;

  auto title_container =
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
  const gfx::ImageSkia* image = prompt_->icon().ToImageSkia();
  gfx::Size size(image->width(), image->height());
  size.SetToMin(gfx::Size(icon_size, icon_size));
  title_container.AddChild(views::Builder<views::ImageView>()
                               .SetImage(ui::ImageModel::FromImageSkia(*image))
                               .SetImageSize(size));

  if (prompt_->has_webstore_data()) {
    title_container.AddChild(CreateWebstoreDataBuilder());
  } else {
    title_container.AddChild(
        views::Builder<TitleLabelWrapper>(std::make_unique<TitleLabelWrapper>(
            views::BubbleFrameView::CreateDefaultTitleLabel(
                prompt_->GetDialogTitle()))));
  }

  return std::move(title_container).Build();
}

views::Builder<views::BoxLayoutView>
ExtensionInstallDialogView::CreateWebstoreDataBuilder() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  // Title
  auto title_label_builder =
      views::Builder<TitleLabelWrapper>(std::make_unique<TitleLabelWrapper>(
          views::BubbleFrameView::CreateDefaultTitleLabel(
              prompt_->GetDialogTitle())));

  // Rating
  auto rating_builder =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetID(ExtensionInstallDialogView::kRatingsViewId)
          .SetAccessibleRole(ax::mojom::Role::kStaticText)
          .SetAccessibleName(GetRatingAccessibleName(prompt_->average_rating(),
                                                     prompt_->rating_count()));
  std::vector<const gfx::ImageSkia*> rating_stars = prompt_->GetRatingStars();
  for (auto star : rating_stars) {
    rating_builder.AddChild(views::Builder<views::ImageView>()
                                .SetImage(ui::ImageModel::FromImageSkia(*star))
                                .SetAccessibleRole(ax::mojom::Role::kNone));
  }

  auto rating_count_builder =
      views::Builder<views::Label>()
          .SetText(prompt_->GetRatingCount())
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_PRIMARY)
          .SetAccessibleRole(ax::mojom::Role::kNone)
          .SetAccessibleName(std::u16string(),
                             ax::mojom::NameFrom::kAttributeExplicitlyEmpty)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto rating_container_builder =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DISTANCE_RELATED_LABEL_HORIZONTAL))
          .AddChildren(rating_builder, rating_count_builder);

  // User count
  auto user_count_builder = views::Builder<views::Label>()
                                .SetText(prompt_->GetUserCount())
                                .SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL)
                                .SetTextStyle(views::style::STYLE_SECONDARY)
                                .SetAutoColorReadabilityEnabled(false)
                                .SetHorizontalAlignment(gfx::ALIGN_LEFT);

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetInsideBorderInsets(gfx::Insets())
      .SetBetweenChildSpacing(
          provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL))
      .AddChildren(title_label_builder, rating_container_builder,
                   user_count_builder);
}

void ExtensionInstallDialogView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  // This should never be triggered if justification_view_ is not initialized.
  DCHECK(justification_view_);

  justification_view_->UpdateLengthLabel();

  // Check if the request button state should actually be updated before calling
  // DialogModeChanged().
  bool is_justification_length_within_limit =
      justification_view_->IsJustificationLengthWithinLimit();
  if (request_button_enabled_ != is_justification_length_within_limit) {
    request_button_enabled_ = is_justification_length_within_limit;
    DialogModelChanged();
  }
}

void ExtensionInstallDialogView::EnableInstallButton() {
  install_button_enabled_ = true;
  DialogModelChanged();
}

BEGIN_METADATA(ExtensionInstallDialogView)
END_METADATA

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::BindRepeating(&ShowExtensionInstallDialogImpl);
}
