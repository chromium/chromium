// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_dialog_view.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

// Time delay before the install button is enabled after initial display.
int g_install_delay_in_ms = 500;

// A custom view to contain the ratings information (stars, ratings count, etc).
// With screen readers, this will handle conveying the information properly
// (i.e., "Rated 4.2 stars by 379 reviews" rather than "image image...379").
class RatingsView : public views::View {
 public:
  RatingsView(double rating, int rating_count)
      : rating_(rating), rating_count_(rating_count) {
    SetID(ExtensionInstallDialogView::kRatingsViewId);
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
  }
  ~RatingsView() override {}

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kStaticText;
    base::string16 accessible_text;
    if (rating_count_ == 0) {
      accessible_text = l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_NO_RATINGS_ACCESSIBLE_TEXT);
    } else {
      accessible_text = base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_RATING_ACCESSIBLE_TEXT),
          rating_, rating_count_);
    }
    node_data->SetName(accessible_text);
  }

 private:
  double rating_;
  int rating_count_;

  DISALLOW_COPY_AND_ASSIGN(RatingsView);
};

// A custom view for the ratings star image that will be ignored by screen
// readers (since the RatingsView handles the context).
class RatingStar : public views::ImageView {
 public:
  explicit RatingStar(const gfx::ImageSkia& image) { SetImage(image); }
  ~RatingStar() override {}

  // views::ImageView:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kIgnored;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RatingStar);
};

// A custom view for the ratings label that will be ignored by screen readers
// (since the RatingsView handles the context).
class RatingLabel : public views::Label {
 public:
  RatingLabel(const base::string16& text, int text_context)
      : views::Label(text, text_context, views::style::STYLE_PRIMARY) {}
  ~RatingLabel() override {}

  // views::Label:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kIgnored;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RatingLabel);
};

// A custom view for the permissions section of the extension info. It contains
// the labels for each permission and the views for their associated details, if
// there are any.
class PermissionsView : public views::View {
 public:
  explicit PermissionsView(int available_width)
      : available_width_(available_width) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  }

  void AddItem(const base::string16& permission_text,
               const base::string16& permission_details) {
    auto permission_label =
        std::make_unique<views::Label>(permission_text, CONTEXT_BODY_TEXT_LARGE,
                                       views::style::STYLE_SECONDARY);
    permission_label->SetMultiLine(true);
    permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    permission_label->SizeToFit(available_width_);
    AddChildView(permission_label.release());
    if (!permission_details.empty()) {
      // If we have more details to provide, show them in collapsed form.
      std::vector<base::string16> details_container;
      details_container.push_back(permission_details);
      AddChildView(
          new ExpandableContainerView(details_container, available_width_));
    }
  }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

 private:
  int available_width_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsView);
};

void AddResourceIcon(const gfx::ImageSkia* skia_image, void* data) {
  views::View* parent = static_cast<views::View*>(data);
  parent->AddChildView(new RatingStar(*skia_image));
}

void ShowExtensionInstallDialogImpl(
    ExtensionInstallPromptShowParams* show_params,
    const ExtensionInstallPrompt::DoneCallback& done_callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      show_params->profile(), show_params->GetParentWebContents(),
      done_callback, std::move(prompt));
  constrained_window::CreateBrowserModalDialogViews(
      dialog, show_params->GetParentWindow())
      ->Show();
}

// A custom scrollable view implementation for the dialog.
class CustomScrollableView : public views::View {
 public:
  explicit CustomScrollableView(ExtensionInstallDialogView* parent)
      : parent_(parent) {}
  ~CustomScrollableView() override {}

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
    parent_->ResizeWidget();
  }

 private:
  // This view is an child of the dialog view (via |scroll_view_|) and thus will
  // not outlive it.
  ExtensionInstallDialogView* parent_;

  DISALLOW_COPY_AND_ASSIGN(CustomScrollableView);
};

// Represents one section in the scrollable info area, which could be a block of
// permissions, a list of retained files, or a list of retained devices.
struct ExtensionInfoSection {
  base::string16 header;
  std::unique_ptr<views::View> contents_view;
};

// Adds a section to |sections| for permissions of |perm_type| if there are any.
void AddPermissions(ExtensionInstallPrompt::Prompt* prompt,
                    std::vector<ExtensionInfoSection>& sections,
                    int available_width) {
  DCHECK_GT(prompt->GetPermissionCount(), 0u);

  auto permissions_view = std::make_unique<PermissionsView>(available_width);

  for (size_t i = 0; i < prompt->GetPermissionCount(); ++i) {
    permissions_view->AddItem(prompt->GetPermission(i),
                              prompt->GetPermissionsDetails(i));
  }

  sections.push_back(
      {prompt->GetPermissionsHeading(), std::move(permissions_view)});
}

std::unique_ptr<views::Link> CreatePromptLink(views::LinkListener* listener) {
  auto store_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_STORE_LINK));
  store_link->set_listener(listener);
  return store_link;
}

}  // namespace

ExtensionInstallDialogView::ExtensionInstallDialogView(
    Profile* profile,
    content::PageNavigator* navigator,
    const ExtensionInstallPrompt::DoneCallback& done_callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt)
    : profile_(profile),
      navigator_(navigator),
      done_callback_(done_callback),
      prompt_(std::move(prompt)),
      title_(prompt_->GetDialogTitle()),
      scroll_view_(nullptr),
      handled_result_(false),
      install_button_enabled_(false) {
  DCHECK(prompt_->extension());

  DialogDelegate::set_default_button(ui::DIALOG_BUTTON_CANCEL);
  DialogDelegate::set_draggable(true);
  if (prompt_->has_webstore_data())
    DialogDelegate::SetExtraView(CreatePromptLink(this));
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   prompt_->GetAcceptButtonLabel());
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   prompt_->GetAbortButtonLabel());
  set_close_on_deactivate(false);
  CreateContents();

  UMA_HISTOGRAM_ENUMERATION("Extensions.InstallPrompt.Type2", prompt_->type(),
                            ExtensionInstallPrompt::NUM_PROMPT_TYPES);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTENSION_INSTALL);
}

ExtensionInstallDialogView::~ExtensionInstallDialogView() {
  if (!handled_result_ && !done_callback_.is_null()) {
    std::move(done_callback_)
        .Run(ExtensionInstallPrompt::Result::USER_CANCELED);
  }
}

void ExtensionInstallDialogView::SetInstallButtonDelayForTesting(
    int delay_in_ms) {
  g_install_delay_in_ms = delay_in_ms;
}

void ExtensionInstallDialogView::ResizeWidget() {
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

gfx::Size ExtensionInstallDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void ExtensionInstallDialogView::VisibilityChanged(views::View* starting_from,
                                                   bool is_visible) {
  if (is_visible) {
    DCHECK(!install_result_timer_);
    install_result_timer_ = base::ElapsedTimer();

    if (!install_button_enabled_) {
      // This base::Unretained is safe because the task is owned by the timer,
      // which is in turn owned by this object.
      enable_install_timer_.Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(g_install_delay_in_ms),
          base::BindOnce(&ExtensionInstallDialogView::EnableInstallButton,
                         base::Unretained(this)));
    }
  }
}

void ExtensionInstallDialogView::AddedToWidget() {
  auto title_container = std::make_unique<views::View>();

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  views::GridLayout* layout =
      title_container->SetLayoutManager(std::make_unique<views::GridLayout>());
  constexpr int kTitleColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kTitleColumnSetId);
  constexpr int icon_size = extension_misc::EXTENSION_ICON_SMALL;
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                        views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                        icon_size, 0);

  // Equalize padding on the left and the right of the icon.
  column_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetInsetsMetric(views::INSETS_DIALOG).left());
  // Set a resize weight so that the title label will be expanded to the
  // available width.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                        1.0, views::GridLayout::USE_PREF, 0, 0);

  // Scale down to icon size, but allow smaller icons (don't scale up).
  const gfx::ImageSkia* image = prompt_->icon().ToImageSkia();
  auto icon = std::make_unique<views::ImageView>();
  gfx::Size size(image->width(), image->height());
  size.SetToMin(gfx::Size(icon_size, icon_size));
  icon->SetImageSize(size);
  icon->SetImage(*image);

  layout->StartRow(views::GridLayout::kFixedSize, kTitleColumnSetId);
  layout->AddView(std::move(icon));

  std::unique_ptr<views::Label> title_label =
      views::BubbleFrameView::CreateDefaultTitleLabel(title_);
  // Setting the title's preferred size to 0 ensures it won't influence the
  // overall size of the dialog. It will be expanded by GridLayout.
  title_label->SetPreferredSize(gfx::Size(0, 0));
  if (prompt_->has_webstore_data()) {
    auto webstore_data_container = std::make_unique<views::View>();
    webstore_data_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical, gfx::Insets(),
            provider->GetDistanceMetric(
                DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));

    webstore_data_container->AddChildView(title_label.release());

    auto rating_container = std::make_unique<views::View>();
    rating_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
    auto rating = std::make_unique<RatingsView>(prompt_->average_rating(),
                                                prompt_->rating_count());
    prompt_->AppendRatingStars(AddResourceIcon, rating.get());
    rating_container->AddChildView(std::move(rating));
    auto rating_count = std::make_unique<RatingLabel>(prompt_->GetRatingCount(),
                                                      CONTEXT_BODY_TEXT_LARGE);
    rating_count->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    rating_container->AddChildView(std::move(rating_count));
    webstore_data_container->AddChildView(std::move(rating_container));

    auto user_count = std::make_unique<views::Label>(
        prompt_->GetUserCount(), CONTEXT_BODY_TEXT_SMALL,
        views::style::STYLE_SECONDARY);
    user_count->SetAutoColorReadabilityEnabled(false);
    user_count->SetEnabledColor(SK_ColorGRAY);
    user_count->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    webstore_data_container->AddChildView(std::move(user_count));

    layout->AddView(std::move(webstore_data_container));
  } else {
    layout->AddView(std::move(title_label));
  }

  GetBubbleFrameView()->SetTitleView(std::move(title_container));
}

bool ExtensionInstallDialogView::Cancel() {
  if (handled_result_)
    return true;

  handled_result_ = true;
  UpdateInstallResultHistogram(false);
  std::move(done_callback_).Run(ExtensionInstallPrompt::Result::USER_CANCELED);
  return true;
}

bool ExtensionInstallDialogView::Accept() {
  DCHECK(!handled_result_);

  handled_result_ = true;
  UpdateInstallResultHistogram(true);
  std::move(done_callback_).Run(ExtensionInstallPrompt::Result::ACCEPTED);
  return true;
}

int ExtensionInstallDialogView::GetDialogButtons() const {
  int buttons = prompt_->GetDialogButtons();
  // Simply having just an OK button is *not* supported. See comment on function
  // GetDialogButtons in dialog_delegate.h for reasons.
  DCHECK_GT(buttons & ui::DIALOG_BUTTON_CANCEL, 0);
  return buttons;
}

bool ExtensionInstallDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return install_button_enabled_;
  return true;
}

bool ExtensionInstallDialogView::ShouldShowCloseButton() const {
  return true;
}

ax::mojom::Role ExtensionInstallDialogView::GetAccessibleWindowRole() {
  return ax::mojom::Role::kAlertDialog;
}

base::string16 ExtensionInstallDialogView::GetAccessibleWindowTitle() const {
  return title_;
}

ui::ModalType ExtensionInstallDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ExtensionInstallDialogView::LinkClicked(views::Link* source,
                                             int event_flags) {
  GURL store_url(extension_urls::GetWebstoreItemDetailURLPrefix() +
                 prompt_->extension()->id());
  OpenURLParams params(store_url, Referrer(),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB,
                       ui::PAGE_TRANSITION_LINK, false);

  if (navigator_) {
    navigator_->OpenURL(params);
  } else {
    chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
    displayer.browser()->OpenURL(params);
  }
  GetWidget()->Close();
}

void ExtensionInstallDialogView::CreateContents() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto extension_info_container = std::make_unique<CustomScrollableView>(this);
  const gfx::Insets content_insets =
      provider->GetDialogInsetsForContentType(views::CONTROL, views::CONTROL);
  extension_info_container->SetBorder(views::CreateEmptyBorder(
      0, content_insets.left(), 0, content_insets.right()));
  extension_info_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  const int content_width = GetPreferredSize().width() -
                            extension_info_container->GetInsets().width();

  std::vector<ExtensionInfoSection> sections;
  if (prompt_->ShouldShowPermissions()) {
    bool has_permissions = prompt_->GetPermissionCount() > 0;
    if (has_permissions) {
      AddPermissions(prompt_.get(), sections, content_width);
    } else {
      sections.push_back(
          {l10n_util::GetStringUTF16(IDS_EXTENSION_NO_SPECIAL_PERMISSIONS),
           nullptr});
    }
  }

  if (prompt_->GetRetainedFileCount()) {
    std::vector<base::string16> details;
    for (size_t i = 0; i < prompt_->GetRetainedFileCount(); ++i) {
      details.push_back(prompt_->GetRetainedFile(i));
    }
    sections.push_back(
        {prompt_->GetRetainedFilesHeading(),
         std::make_unique<ExpandableContainerView>(details, content_width)});
  }

  if (prompt_->GetRetainedDeviceCount()) {
    std::vector<base::string16> details;
    for (size_t i = 0; i < prompt_->GetRetainedDeviceCount(); ++i) {
      details.push_back(prompt_->GetRetainedDeviceMessageString(i));
    }
    sections.push_back(
        {prompt_->GetRetainedDevicesHeading(),
         std::make_unique<ExpandableContainerView>(details, content_width)});
  }

  if (sections.empty()) {
    // Use a smaller margin between the title area and buttons, since there
    // isn't any content.
    set_margins(gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                            0, 0, 0));
    return;
  }

  set_margins(gfx::Insets(content_insets.top(), 0, content_insets.bottom(), 0));

  for (ExtensionInfoSection& section : sections) {
    views::Label* header_label =
        new views::Label(section.header, CONTEXT_BODY_TEXT_LARGE);
    header_label->SetMultiLine(true);
    header_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    header_label->SizeToFit(content_width);
    extension_info_container->AddChildView(header_label);

    if (section.contents_view)
      extension_info_container->AddChildView(section.contents_view.release());
  }

  scroll_view_ = new views::ScrollView();
  scroll_view_->SetHideHorizontalScrollBar(true);
  scroll_view_->SetContents(std::move(extension_info_container));
  scroll_view_->ClipHeightTo(
      0, provider->GetDistanceMetric(
             views::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT));
  AddChildView(scroll_view_);
}

void ExtensionInstallDialogView::EnableInstallButton() {
  install_button_enabled_ = true;
  DialogModelChanged();
}

void ExtensionInstallDialogView::UpdateInstallResultHistogram(bool accepted)
    const {
  // Only update histograms if |install_result_timer_| was initialized in
  // |VisibilityChanged|.
  if (prompt_->type() == ExtensionInstallPrompt::INSTALL_PROMPT &&
      install_result_timer_) {
    if (accepted) {
      UmaHistogramMediumTimes("Extensions.InstallPrompt.TimeToInstall",
                              install_result_timer_->Elapsed());
    } else {
      UmaHistogramMediumTimes("Extensions.InstallPrompt.TimeToCancel",
                              install_result_timer_->Elapsed());
    }
  }
}


// ExpandableContainerView::DetailsView ----------------------------------------

ExpandableContainerView::DetailsView::DetailsView(
    const std::vector<base::string16>& details) {
  // Spacing between this and the "Hide Details" link.
  const int bottom_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, 0, bottom_padding, 0),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));

  for (auto& detail : details) {
    auto detail_label = std::make_unique<views::Label>(
        detail, CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_SECONDARY);
    detail_label->SetMultiLine(true);
    detail_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(detail_label.release());
  }
}

gfx::Size ExpandableContainerView::DetailsView::CalculatePreferredSize() const {
  return expanded_ ? views::View::CalculatePreferredSize() : gfx::Size();
}

void ExpandableContainerView::DetailsView::ToggleExpanded() {
  expanded_ = !expanded_;
  PreferredSizeChanged();
}

// ExpandableContainerView -----------------------------------------------------

ExpandableContainerView::ExpandableContainerView(
    const std::vector<base::string16>& details,
    int available_width)
    : details_view_(nullptr), details_link_(nullptr) {
  DCHECK(!details.empty());

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  constexpr int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);

  // Even though we only have one column, using a GridLayout here will
  // properly handle a 0 height row when |details_view_| is collapsed.
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                        views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                        available_width, 0);

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  details_view_ = layout->AddView(std::make_unique<DetailsView>(details));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  auto details_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
  details_link->set_listener(this);
  details_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  details_link_ = layout->AddView(std::move(details_link));
}

ExpandableContainerView::~ExpandableContainerView() {
}

void ExpandableContainerView::LinkClicked(
    views::Link* source, int event_flags) {
  ToggleDetailLevel();
}

void ExpandableContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ExpandableContainerView::ToggleDetailLevel() {
  details_view_->ToggleExpanded();
  details_link_->SetText(l10n_util::GetStringUTF16(
      details_view_->expanded() ? IDS_EXTENSIONS_HIDE_DETAILS
                                : IDS_EXTENSIONS_SHOW_DETAILS));
}

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::Bind(&ShowExtensionInstallDialogImpl);
}
