// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing/sharing_app.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "components/sync_device_info/device_info.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#endif

namespace {

constexpr int kSharingDialogSpacing = 8;

SkColor GetColorFromTheme() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  return native_theme->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
}

std::unique_ptr<views::ImageView> CreateIconView(const gfx::ImageSkia& icon) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(icon);
  return icon_view;
}

gfx::ImageSkia CreateVectorIcon(const gfx::VectorIcon& vector_icon) {
  constexpr int kPrimaryIconSize = 20;
  return gfx::CreateVectorIcon(vector_icon, kPrimaryIconSize,
                               GetColorFromTheme());
}

gfx::ImageSkia CreateDeviceIcon(
    const sync_pb::SyncEnums::DeviceType device_type) {
  return CreateVectorIcon(device_type == sync_pb::SyncEnums::TYPE_TABLET
                              ? kTabletIcon
                              : kHardwareSmartphoneIcon);
}

gfx::ImageSkia CreateAppIcon(const SharingApp& app) {
  return app.vector_icon ? CreateVectorIcon(*app.vector_icon)
                         : app.image.AsImageSkia();
}

// TODO(himanshujaju): This is almost same as self share, we could unify these
// methods once we unify our architecture and dialog views.
base::string16 GetLastUpdatedTimeInDays(base::Time last_updated_timestamp) {
  int time_in_days = (base::Time::Now() - last_updated_timestamp).InDays();
  return l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_SHARING_DIALOG_DEVICE_SUBTITLE_LAST_ACTIVE_DAYS,
      time_in_days);
}

bool ShouldShowOrigin(const SharingDialogData& data,
                      content::WebContents* web_contents) {
  return data.initiating_origin &&
         !data.initiating_origin->IsSameOriginWith(
             web_contents->GetMainFrame()->GetLastCommittedOrigin());
}

base::string16 PrepareHelpTextWithoutOrigin(const SharingDialogData& data,
                                            const base::string16& link,
                                            size_t* link_offset) {
  DCHECK_NE(0, data.help_text_id);
  return l10n_util::GetStringFUTF16(data.help_text_id, link, link_offset);
}

base::string16 PrepareHelpTextWithOrigin(const SharingDialogData& data,
                                         const base::string16& link,
                                         size_t* link_offset) {
  DCHECK_NE(0, data.help_text_origin_id);
  base::string16 origin = url_formatter::FormatOriginForSecurityDisplay(
      *data.initiating_origin,
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  std::vector<size_t> offsets;
  base::string16 text = l10n_util::GetStringFUTF16(data.help_text_origin_id,
                                                   {origin, link}, &offsets);
  *link_offset = offsets[1];
  return text;
}

std::unique_ptr<views::StyledLabel> CreateHelpText(
    const SharingDialogData& data,
    views::StyledLabelListener* listener,
    bool show_origin) {
  DCHECK_NE(0, data.help_link_text_id);
  const base::string16 link = l10n_util::GetStringUTF16(data.help_link_text_id);
  size_t offset;
  const base::string16 text =
      show_origin ? PrepareHelpTextWithOrigin(data, link, &offset)
                  : PrepareHelpTextWithoutOrigin(data, link, &offset);
  auto label = std::make_unique<views::StyledLabel>(text, listener);
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  label->AddStyleRange(gfx::Range(offset, offset + link.length()), link_style);
  return label;
}

std::unique_ptr<views::View> CreateOriginView(const SharingDialogData& data) {
  DCHECK(data.initiating_origin);
  DCHECK_NE(0, data.origin_text_id);
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          data.origin_text_id,
          url_formatter::FormatOriginForSecurityDisplay(
              *data.initiating_origin,
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)),
      ChromeTextContext::CONTEXT_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  label->SetMultiLine(true);
  return label;
}

std::unique_ptr<views::View> MaybeCreateImageView(
    const gfx::VectorIcon* image) {
  if (!image)
    return nullptr;

  constexpr gfx::Size kHeaderImageSize(320, 100);

  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetPreferredSize(kHeaderImageSize);
  image_view->SetImage(gfx::CreateVectorIcon(*image, gfx::kPlaceholderColor));
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  return image_view;
}

std::unique_ptr<views::View> CreateFootnoteView(
    const SharingDialogData& data,
    content::WebContents* web_contents,
    views::StyledLabelListener* listener) {
  bool show_origin = ShouldShowOrigin(data, web_contents);
  switch (data.type) {
    case SharingDialogType::kDialogWithoutDevicesWithApp:
      return CreateHelpText(data, listener, show_origin);
    case SharingDialogType::kDialogWithDevicesMaybeApps:
      return show_origin ? CreateOriginView(data) : nullptr;
    case SharingDialogType::kErrorDialog:
    case SharingDialogType::kEducationalDialog:
      return nullptr;
  }
}

}  // namespace

SharingDialogView::SharingDialogView(views::View* anchor_view,
                                     content::WebContents* web_contents,
                                     SharingDialogData data)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      data_(std::move(data)) {
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
  DialogDelegate::SetFootnoteView(
      ::CreateFootnoteView(data_, web_contents, this));
}

SharingDialogView::~SharingDialogView() = default;

void SharingDialogView::Hide() {
  CloseBubble();
}

void SharingDialogView::StyledLabelLinkClicked(views::StyledLabel* label,
                                               const gfx::Range& range,
                                               int event_flags) {
  std::move(data_.help_callback).Run(GetDialogType());
  CloseBubble();
}

SharingDialogType SharingDialogView::GetDialogType() const {
  return data_.type;
}

void SharingDialogView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  button_icons_.clear();

  auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets insets =
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT);

  SharingDialogType type = GetDialogType();
  LogSharingDialogShown(data_.prefix, type);

  switch (type) {
    case SharingDialogType::kErrorDialog:
      InitErrorView();
      break;
    case SharingDialogType::kEducationalDialog:
      InitEmptyView();
      break;
    case SharingDialogType::kDialogWithoutDevicesWithApp:
    case SharingDialogType::kDialogWithDevicesMaybeApps:
      // Spread buttons across the whole dialog width.
      insets = gfx::Insets(kSharingDialogSpacing, 0, kSharingDialogSpacing, 0);
      InitListView();
      break;
  }

  set_margins(gfx::Insets(insets.top(), 0, insets.bottom(), 0));
  SetBorder(views::CreateEmptyBorder(0, insets.left(), 0, insets.right()));

  if (GetWidget())
    SizeToContents();
}

void SharingDialogView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  DCHECK(data_.device_callback);
  DCHECK(data_.app_callback);
  if (!sender || sender->tag() < 0)
    return;
  size_t index{sender->tag()};

  if (index < data_.devices.size()) {
    LogSharingSelectedDeviceIndex(data_.prefix, kSharingUiDialog, index);
    std::move(data_.device_callback).Run(*data_.devices[index]);
    CloseBubble();
    return;
  }

  index -= data_.devices.size();

  if (index < data_.apps.size()) {
    LogSharingSelectedAppIndex(data_.prefix, kSharingUiDialog, index);
    std::move(data_.app_callback).Run(data_.apps[index]);
    CloseBubble();
  }
}

void SharingDialogView::MaybeShowHeaderImage() {
  views::BubbleFrameView* frame_view = GetBubbleFrameView();
  if (!frame_view)
    return;

  // TODO(crbug.com/1013099): Merge both images using alpha blending so they
  // work on any background color.
  const gfx::VectorIcon* image =
      color_utils::IsDark(frame_view->GetBackgroundColor())
          ? data_.header_image_dark
          : data_.header_image_light;

  frame_view->SetHeaderView(MaybeCreateImageView(image));
}

void SharingDialogView::AddedToWidget() {
  MaybeShowHeaderImage();
}

void SharingDialogView::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();
  MaybeShowHeaderImage();

  if (!button_icons_.size())
    return;

  DCHECK_EQ(data_.devices.size() + data_.apps.size(), button_icons_.size());

  size_t button_index = 0;
  for (const auto& device : data_.devices) {
    button_icons_[button_index]->SetImage(
        CreateDeviceIcon(device->device_type()));
    button_index++;
  }
  for (const auto& app : data_.apps) {
    button_icons_[button_index]->SetImage(CreateAppIcon(app));
    button_index++;
  }
}

void SharingDialogView::InitListView() {
  int tag = 0;
  const gfx::Insets device_border =
      gfx::Insets(kSharingDialogSpacing, kSharingDialogSpacing * 2,
                  kSharingDialogSpacing, 0);
  // Apps need more padding at the top and bottom as they only have one line.
  const gfx::Insets app_border = device_border + gfx::Insets(2, 0, 2, 0);

  // Devices:
  LogSharingDevicesToShow(data_.prefix, kSharingUiDialog, data_.devices.size());
  for (const auto& device : data_.devices) {
    auto icon = CreateIconView(CreateDeviceIcon(device->device_type()));
    button_icons_.push_back(icon.get());
    auto dialog_button = std::make_unique<HoverButton>(
        this, std::move(icon), base::UTF8ToUTF16(device->client_name()),
        GetLastUpdatedTimeInDays(device->last_updated_timestamp()));
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_button->SetBorder(views::CreateEmptyBorder(device_border));
    dialog_buttons_.push_back(AddChildView(std::move(dialog_button)));
  }

  // Apps:
  LogSharingAppsToShow(data_.prefix, kSharingUiDialog, data_.apps.size());
  for (const auto& app : data_.apps) {
    auto icon = CreateIconView(CreateAppIcon(app));
    button_icons_.push_back(icon.get());
    auto dialog_button =
        std::make_unique<HoverButton>(this, std::move(icon), app.name,
                                      /* subtitle= */ base::string16());
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_button->SetBorder(views::CreateEmptyBorder(app_border));
    dialog_buttons_.push_back(AddChildView(std::move(dialog_button)));
  }
}

void SharingDialogView::InitEmptyView() {
  bool show_origin = ShouldShowOrigin(data_, web_contents());
  AddChildView(CreateHelpText(data_, this, show_origin));
}

void SharingDialogView::InitErrorView() {
  auto label = std::make_unique<views::Label>(data_.error_text,
                                              views::style::CONTEXT_LABEL,
                                              views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  AddChildView(std::move(label));
}

gfx::Size SharingDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

bool SharingDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 SharingDialogView::GetWindowTitle() const {
  return data_.title;
}

void SharingDialogView::WebContentsDestroyed() {
  LocationBarBubbleDelegateView::WebContentsDestroyed();
  // Call the close callback here already so we can log metrics for closed
  // dialogs before the controller is destroyed.
  WindowClosing();
}

void SharingDialogView::WindowClosing() {
  if (data_.close_callback)
    std::move(data_.close_callback).Run(this);
}

// static
views::BubbleDialogDelegateView* SharingDialogView::GetAsBubble(
    SharingDialog* dialog) {
  return static_cast<SharingDialogView*>(dialog);
}

// static
views::BubbleDialogDelegateView* SharingDialogView::GetAsBubbleForClickToCall(
    SharingDialog* dialog) {
#if defined(OS_CHROMEOS)
  if (!dialog) {
    auto* bubble = IntentPickerBubbleView::intent_picker_bubble();
    if (bubble && bubble->icon_type() == PageActionIconType::kClickToCall)
      return bubble;
  }
#endif
  return static_cast<SharingDialogView*>(dialog);
}
