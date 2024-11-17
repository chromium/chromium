// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_metrics.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_footer_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_legacy_cast_footer_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/soda/constants.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/url_util.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"

using global_media_controls::GlobalMediaControlsEntryPoint;
using media_session::mojom::MediaSessionAction;

namespace {

static constexpr int kHorizontalMarginDip = 20;
static constexpr int kImageWidthDip = 20;
static constexpr int kVerticalMarginDip = 10;

std::u16string GetLiveCaptionTitle(PrefService* profile_prefs) {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    return l10n_util::GetStringUTF16(
        IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_ENGLISH_ONLY);
  }
  // The selected language is only shown when Live Caption is enabled.
  if (profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled)) {
    std::u16string language = speech::GetLanguageDisplayName(
        prefs::GetLiveCaptionLanguageCode(profile_prefs),
        g_browser_process->GetApplicationLocale());
    return l10n_util::GetStringFUTF16(
        IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_SHOW_LANGUAGE, language);
  }
  return l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION);
}

}  // namespace

// static
MediaDialogView* MediaDialogView::instance_ = nullptr;

// static
bool MediaDialogView::has_been_opened_ = false;

// static
views::Widget* MediaDialogView::ShowDialogFromToolbar(
    views::View* anchor_view,
    MediaNotificationService* service,
    Profile* profile) {
  return ShowDialog(
      anchor_view, views::BubbleBorder::TOP_RIGHT, service, profile, nullptr,
      global_media_controls::GlobalMediaControlsEntryPoint::kToolbarIcon);
}

// static
views::Widget* MediaDialogView::ShowDialogCentered(
    const gfx::Rect& bounds,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  auto* widget = ShowDialog(nullptr, views::BubbleBorder::TOP_CENTER, service,
                            profile, contents, entry_point);
  instance_->SetAnchorRect(bounds);
  return widget;
}

// static
views::Widget* MediaDialogView::ShowDialog(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_position,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  DCHECK(service);
  // Hide the previous instance if it exists, since there can only be one dialog
  // instance at a time.
  HideDialog();
  instance_ = new MediaDialogView(anchor_view, anchor_position, service,
                                  profile, contents, entry_point);
  if (!anchor_view) {
    instance_->set_has_parent(false);
  }

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();

  base::UmaHistogramBoolean("Media.GlobalMediaControls.RepeatUsage",
                            has_been_opened_);
  base::UmaHistogramEnumeration("Media.GlobalMediaControls.EntryPoint",
                                entry_point);
  has_been_opened_ = true;

  return widget;
}

// static
void MediaDialogView::HideDialog() {
  if (IsShowing()) {
    instance_->service_->media_item_manager()->SetDialogDelegate(nullptr);
    speech::SodaInstaller::GetInstance()->RemoveObserver(instance_);
    instance_->GetWidget()->Close();
  }

  // Set |instance_| to nullptr so that |IsShowing()| returns false immediately.
  // We also set to nullptr in |WindowClosing()| (which happens asynchronously),
  // since |HideDialog()| is not always called.
  instance_ = nullptr;
}

// static
bool MediaDialogView::IsShowing() {
  return instance_ != nullptr;
}

global_media_controls::MediaItemUI* MediaDialogView::ShowMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  global_media_controls::MediaItemUI* view_ptr;

  if (media_color_theme_.has_value()) {
    auto view = BuildMediaItemUIUpdatedView(id, item);
    view_ptr = view.get();
    updated_items_[id] = view.get();
    active_sessions_view_->ShowUpdatedItem(id, std::move(view));
  } else {
    auto view = BuildMediaItemUIView(id, item);
    view_ptr = view.get();
    observed_items_[id] = view.get();
    active_sessions_view_->ShowItem(id, std::move(view));
  }

  view_ptr->AddObserver(this);
  UpdateBubbleSize();
  observers_.Notify(&MediaDialogViewObserver::OnMediaSessionShown);
  return view_ptr;
}

void MediaDialogView::HideMediaItem(const std::string& id) {
  if (media_color_theme_.has_value()) {
    active_sessions_view_->HideUpdatedItem(id);
  } else {
    active_sessions_view_->HideItem(id);
  }

  if (active_sessions_view_->empty()) {
    HideDialog();
  } else {
    UpdateBubbleSize();
  }

  observers_.Notify(&MediaDialogViewObserver::OnMediaSessionHidden);
}

void MediaDialogView::RefreshMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (observed_items_.find(id) == observed_items_.end() &&
      updated_items_.find(id) == updated_items_.end()) {
    return;
  }
  bool show_devices =
      entry_point_ == GlobalMediaControlsEntryPoint::kPresentation;

  if (media_color_theme_.has_value()) {
    updated_items_[id]->UpdateFooterView(
        BuildFooter(id, item, profile_, media_color_theme_));
    updated_items_[id]->UpdateDeviceSelectorView(
        BuildDeviceSelector(id, item, service_, service_, profile_,
                            entry_point_, show_devices, media_color_theme_));
  } else {
    observed_items_[id]->UpdateFooterView(
        BuildFooter(id, item, profile_, media_color_theme_));
    observed_items_[id]->UpdateDeviceSelector(
        BuildDeviceSelector(id, item, service_, service_, profile_,
                            entry_point_, show_devices, media_color_theme_));
  }

  UpdateBubbleSize();
}

void MediaDialogView::HideMediaDialog() {
  HideDialog();
}

void MediaDialogView::Focus() {
  RequestFocus();
}

void MediaDialogView::AddedToWidget() {
  int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  views::BubbleFrameView* frame = GetBubbleFrameView();
  if (frame) {
    frame->SetCornerRadius(corner_radius);
  }
  if (entry_point_ ==
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation) {
    service_->SetDialogDelegateForWebContents(
        this, web_contents_for_presentation_request_);
  } else {
    service_->media_item_manager()->SetDialogDelegate(this);
  }
  speech::SodaInstaller::GetInstance()->AddObserver(this);
}

gfx::Size MediaDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // If we have active sessions, then fit to them.
  if (!active_sessions_view_->empty()) {
    return views::BubbleDialogDelegateView::CalculatePreferredSize(
        available_size);
  }
  // Otherwise, use a standard size for bubble dialogs.
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, 1);
}

void MediaDialogView::UpdateBubbleSize() {
  SizeToContents();
  if (!captions::IsLiveCaptionFeatureSupported()) {
    return;
  }
  const int width = active_sessions_view_->GetPreferredSize().width();
  const int live_caption_height =
      live_caption_container_->GetPreferredSize().height();
  live_caption_container_->SetPreferredSize(
      gfx::Size(width, live_caption_height));

  if (media::IsLiveTranslateEnabled()) {
    const int live_translate_height =
        live_translate_container_->GetPreferredSize().height();
    live_translate_container_->SetPreferredSize(
        gfx::Size(width, live_translate_height));

    live_translate_label_wrapper_->SetPreferredSize(gfx::Size(
        width, live_translate_label_wrapper_->GetPreferredSize().height()));

    // Align the combo box with the text labels.
    target_language_container_->SetPreferredSize(gfx::Size(
        width, target_language_container_->GetPreferredSize().height()));
    target_language_combobox_->SetPreferredSize(
        gfx::Size(width - 2 * (kImageWidthDip + kHorizontalMarginDip +
                               ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL)),
                  target_language_combobox_->GetPreferredSize().height()));

    separator_->SetPreferredLength(width - 2 * kHorizontalMarginDip);
    caption_settings_button_->SetPreferredSize(
        gfx::Size(width, live_caption_height));
  }
}

void MediaDialogView::OnLiveCaptionEnabledChanged() {
  bool enabled = profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled);

  // Do not update the title if SODA is currently downloading.
  if (!speech::SodaInstaller::GetInstance()->IsSodaDownloading(
          speech::GetLanguageCode(
              prefs::GetLiveCaptionLanguageCode(profile_->GetPrefs())))) {
    SetLiveCaptionTitle(GetLiveCaptionTitle(profile_->GetPrefs()));
  }

  live_caption_button_->SetIsOn(enabled);

  if (media::IsLiveTranslateEnabled()) {
    live_translate_container_->SetVisible(enabled);
  }

  UpdateBubbleSize();
}

void MediaDialogView::OnLiveTranslateEnabledChanged() {
  bool enabled = profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled);
  live_translate_button_->SetIsOn(enabled);
  target_language_container_->SetVisible(enabled);
  UpdateBubbleSize();
}

void MediaDialogView::OnMediaItemUISizeChanged() {
  UpdateBubbleSize();
}

void MediaDialogView::OnMediaItemUIMetadataChanged() {
  observers_.Notify(&MediaDialogViewObserver::OnMediaSessionMetadataUpdated);
}

void MediaDialogView::OnMediaItemUIActionsChanged() {
  observers_.Notify(&MediaDialogViewObserver::OnMediaSessionActionsChanged);
}

void MediaDialogView::OnMediaItemUIDestroyed(const std::string& id) {
  if (media_color_theme_.has_value()) {
    updated_items_.erase(id);
  } else {
    observed_items_.erase(id);
  }
}

void MediaDialogView::AddObserver(MediaDialogViewObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaDialogView::RemoveObserver(MediaDialogViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaDialogView::TargetLanguageChanged() {
  static_cast<LiveTranslateComboboxModel*>(
      target_language_combobox_->GetModel())
      ->UpdateTargetLanguageIndex(
          target_language_combobox_->GetSelectedIndex().value());
}

const std::map<
    const std::string,
    raw_ptr<global_media_controls::MediaItemUIView, CtnExperimental>>&
MediaDialogView::GetItemsForTesting() const {
  return active_sessions_view_->items_for_testing();  // IN-TEST
}

const std::map<
    const std::string,
    raw_ptr<global_media_controls::MediaItemUIUpdatedView, CtnExperimental>>&
MediaDialogView::GetUpdatedItemsForTesting() const {
  return active_sessions_view_->updated_items_for_testing();  // IN-TEST
}

const global_media_controls::MediaItemUIListView*
MediaDialogView::GetListViewForTesting() const {
  return active_sessions_view_;
}

MediaDialogView::MediaDialogView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_position,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point)
    : BubbleDialogDelegateView(anchor_view, anchor_position),
      service_(service),
      profile_(profile->GetOriginalProfile()),
      active_sessions_view_(AddChildView(
          std::make_unique<global_media_controls::MediaItemUIListView>())),
      web_contents_for_presentation_request_(contents),
      entry_point_(entry_point) {
  SetProperty(views::kElementIdentifierKey, kToolbarMediaBubbleElementId);
  // Enable layer based clipping to ensure children using layers are clipped
  // appropriately.
  SetPaintClientToLayer(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_DIALOG_NAME));
  DCHECK(service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&MediaDialogView::OnLiveCaptionEnabledChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveTranslateEnabled,
      base::BindRepeating(&MediaDialogView::OnLiveTranslateEnabledChanged,
                          base::Unretained(this)));

#if !BUILDFLAG(IS_CHROMEOS)
  // MediaDialogView can be built on CrOS but the updated UI should only be
  // enabled for non-CrOS platforms.
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsUpdatedUI)) {
    media_color_theme_ = GetMediaColorTheme();
  }
#endif
}

MediaDialogView::~MediaDialogView() {
  for (auto item_pair : observed_items_) {
    item_pair.second->RemoveObserver(this);
  }
  for (auto item_pair : updated_items_) {
    item_pair.second->RemoveObserver(this);
  }
}

void MediaDialogView::Init() {
  // Remove margins.
  set_margins(gfx::Insets());
  if (!captions::IsLiveCaptionFeatureSupported()) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    return;
  }
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  InitializeLiveCaptionSection();
  if (media::IsLiveTranslateEnabled()) {
    InitializeLiveTranslateSection();

    separator_ = AddChildView(std::make_unique<views::Separator>());
    separator_->SetOrientation(views::Separator::Orientation::kHorizontal);
    InitializeCaptionSettingsSection();
  }
}

void MediaDialogView::WindowClosing() {
  if (instance_ == this) {
    instance_ = nullptr;
    service_->media_item_manager()->SetDialogDelegate(nullptr);
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  }
}

void MediaDialogView::OnLiveCaptionButtonPressed() {
  bool enabled = !profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled);
  profile_->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, enabled);
  base::UmaHistogramBoolean(
      "Accessibility.LiveCaption.EnableFromGlobalMediaControls", enabled);
}

void MediaDialogView::OnLiveTranslateButtonPressed() {
  bool enabled =
      !profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled);
  profile_->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled, enabled);
  base::UmaHistogramBoolean(
      "Accessibility.LiveTranslate.EnableFromGlobalMediaControls", enabled);
}

void MediaDialogView::OnSettingsButtonPressed() {
  NavigateParams navigate_params(profile_,
                                 GURL(captions::GetCaptionSettingsUrl()),
                                 ui::PAGE_TRANSITION_LINK);
  navigate_params.window_action = NavigateParams::WindowAction::SHOW_WINDOW;
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
}

void MediaDialogView::OnSodaInstalled(speech::LanguageCode language_code) {
  if (!prefs::IsLanguageCodeForLiveCaption(language_code,
                                           profile_->GetPrefs())) {
    return;
  }
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  SetLiveCaptionTitle(GetLiveCaptionTitle(profile_->GetPrefs()));
}

void MediaDialogView::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  // Check that language code matches the selected language for Live Caption or
  // is LanguageCode::kNone (signifying the SODA binary failed).
  if (!prefs::IsLanguageCodeForLiveCaption(language_code,
                                           profile_->GetPrefs()) &&
      language_code != speech::LanguageCode::kNone) {
    return;
  }

  std::u16string error_message;
  switch (error_code) {
    case speech::SodaInstaller::ErrorCode::kUnspecifiedError: {
      error_message = l10n_util::GetStringUTF16(
          IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_DOWNLOAD_ERROR);
      break;
    }
    case speech::SodaInstaller::ErrorCode::kNeedsReboot: {
      error_message = l10n_util::GetStringUTF16(
          IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_DOWNLOAD_ERROR_REBOOT_REQUIRED);
      break;
    }
  }

  SetLiveCaptionTitle(error_message);
}

void MediaDialogView::OnSodaProgress(speech::LanguageCode language_code,
                                     int progress) {
  // Check that language code matches the selected language for Live Caption or
  // is LanguageCode::kNone (signifying the SODA binary has progress).
  if (!prefs::IsLanguageCodeForLiveCaption(language_code,
                                           profile_->GetPrefs()) &&
      language_code != speech::LanguageCode::kNone) {
    return;
  }
  SetLiveCaptionTitle(l10n_util::GetStringFUTF16Int(
      IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_DOWNLOAD_PROGRESS, progress));
}

void MediaDialogView::InitializeLiveCaptionSection() {
  auto live_caption_container = std::make_unique<View>();

  auto live_caption_image = std::make_unique<views::ImageView>();
  live_caption_image->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kLiveCaptionOnIcon, ui::kColorIcon, kImageWidthDip));
  live_caption_container->AddChildView(std::move(live_caption_image));

  auto live_caption_title =
      std::make_unique<views::Label>(GetLiveCaptionTitle(profile_->GetPrefs()));
  live_caption_title->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  live_caption_title->SetMultiLine(true);
  live_caption_title_ =
      live_caption_container->AddChildView(std::move(live_caption_title));

  auto live_caption_button = std::make_unique<views::ToggleButton>(
      base::BindRepeating(&MediaDialogView::OnLiveCaptionButtonPressed,
                          base::Unretained(this)));
  live_caption_button->SetIsOn(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  live_caption_button->GetViewAccessibility().SetName(
      live_caption_title_->GetText());
  live_caption_button_ =
      live_caption_container->AddChildView(std::move(live_caption_button));

  auto* live_caption_container_layout =
      live_caption_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets::VH(kVerticalMarginDip, kHorizontalMarginDip),
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL)));
  live_caption_container_layout->SetFlexForView(live_caption_title_, 1);
  live_caption_container_ = AddChildView(std::move(live_caption_container));
}

void MediaDialogView::InitializeLiveTranslateSection() {
  auto live_translate_container = std::make_unique<View>();
  live_translate_container->SetVisible(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));

  auto live_translate_image = std::make_unique<views::ImageView>();
  live_translate_image->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kTranslateIcon, ui::kColorIcon, kImageWidthDip));
  live_translate_container->AddChildView(std::move(live_translate_image));

  auto live_translate_label_wrapper = std::make_unique<View>();
  live_translate_label_wrapper->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  auto live_translate_title =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_TRANSLATE_TITLE));
  live_translate_title->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  live_translate_title->SetMultiLine(true);
  live_translate_title_ = live_translate_label_wrapper->AddChildView(
      std::move(live_translate_title));

  live_translate_label_wrapper_ = live_translate_container->AddChildView(
      std::move(live_translate_label_wrapper));

  auto live_translate_button = std::make_unique<views::ToggleButton>(
      base::BindRepeating(&MediaDialogView::OnLiveTranslateButtonPressed,
                          base::Unretained(this)));
  live_translate_button->SetIsOn(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled));
  live_translate_button->GetViewAccessibility().SetName(
      live_translate_title_->GetText());
  auto* live_translate_container_layout =
      live_translate_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets::VH(kVerticalMarginDip, kHorizontalMarginDip),
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL)));
  live_translate_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  live_translate_container_layout->SetFlexForView(live_translate_label_wrapper_,
                                                  1);
  live_translate_button_ =
      live_translate_container->AddChildView(std::move(live_translate_button));
  live_translate_container_ = AddChildView(std::move(live_translate_container));

  // Initialize the target language container.
  auto target_language_container = std::make_unique<View>();
  target_language_container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, kVerticalMarginDip, 0)));
  target_language_container->SetVisible(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled));
  target_language_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  auto target_language_model =
      std::make_unique<LiveTranslateComboboxModel>(profile_);
  auto target_language_combobox =
      std::make_unique<views::Combobox>(std::move(target_language_model));
  target_language_combobox->SetCallback(base::BindRepeating(
      &MediaDialogView::TargetLanguageChanged, base::Unretained(this)));
  target_language_combobox->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_GLOBAL_MEDIA_CONTROLS_LIVE_TRANSLATE_TARGET_LANGUAGE_ACCNAME));
  target_language_combobox_ = target_language_container->AddChildView(
      std::move(target_language_combobox));
  target_language_container_ =
      AddChildView(std::move(target_language_container));
}

void MediaDialogView::InitializeCaptionSettingsSection() {
  auto caption_settings_container = std::make_unique<View>();
  caption_settings_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(kVerticalMarginDip, 0)));
  auto caption_settings_button = std::make_unique<RichHoverButton>(
      base::BindRepeating(&MediaDialogView::OnSettingsButtonPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsChromeRefreshIcon,
                                     ui::kColorIcon, kImageWidthDip),
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_CAPTION_SETTINGS),
      std::u16string(), std::u16string(), std::u16string(),
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon, ui::kColorIcon,
                                     kImageWidthDip));
  caption_settings_button_ = caption_settings_container->AddChildView(
      std::move(caption_settings_button));
  caption_settings_container_ =
      AddChildView(std::move(caption_settings_container));
}

void MediaDialogView::SetLiveCaptionTitle(const std::u16string& new_text) {
  live_caption_title_->SetText(new_text);
  UpdateBubbleSize();
}

std::unique_ptr<global_media_controls::MediaItemUIView>
MediaDialogView::BuildMediaItemUIView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  bool show_devices =
      entry_point_ == GlobalMediaControlsEntryPoint::kPresentation;
  return std::make_unique<global_media_controls::MediaItemUIView>(
      id, item, BuildFooter(id, item, profile_, media_color_theme_),
      BuildDeviceSelector(id, item, service_, service_, profile_, entry_point_,
                          show_devices, media_color_theme_),
      /*notification_theme=*/std::nullopt, media_color_theme_,
      global_media_controls::MediaDisplayPage::kMediaDialogView);
}

std::unique_ptr<global_media_controls::MediaItemUIUpdatedView>
MediaDialogView::BuildMediaItemUIUpdatedView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  CHECK(media_color_theme_);
  bool show_devices =
      entry_point_ == GlobalMediaControlsEntryPoint::kPresentation;
  return std::make_unique<global_media_controls::MediaItemUIUpdatedView>(
      id, item, media_color_theme_.value(),
      BuildDeviceSelector(id, item, service_, service_, profile_, entry_point_,
                          show_devices, media_color_theme_),
      BuildFooter(id, item, profile_, media_color_theme_));
}

BEGIN_METADATA(MediaDialogView)
END_METADATA
