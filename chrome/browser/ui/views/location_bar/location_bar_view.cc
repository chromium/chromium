// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_icon_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/performance_manager/public/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/sharing_message/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/language_state.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/feature_switch.h"
#include "services/device/public/cpp/device_features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#endif

namespace {

int IncrementalMinimumWidth(const views::View* view) {
  return (view && view->GetVisible()) ? view->GetMinimumSize().width() : 0;
}

// Whether the omnibox enables either of 2 prefix autocompletion features.
bool OmniboxPrefixRichAutocompletionEnabled() {
  return OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixAll.Get() ||
         OmniboxFieldTrial::
             kRichAutocompletionAutocompleteNonPrefixShortcutProvider.Get();
}

// The padding between the intent chip and the other trailing decorations.
constexpr int kIntentChipIntraItemPadding = 12;

// The padding between the content setting icons and other trailing decorations.
constexpr int kContentSettingIntraItemPadding = 8;

}  // namespace

using content::WebContents;
using metrics::OmniboxEventProto;
using views::View;

// LocationBarView -----------------------------------------------------------

LocationBarView::LocationBarView(Browser* browser,
                                 Profile* profile,
                                 CommandUpdater* command_updater,
                                 Delegate* delegate,
                                 bool is_popup_mode)
    : LocationBar(command_updater),
      AnimationDelegateViews(this),
      browser_(browser),
      profile_(profile),
      delegate_(delegate),
      is_popup_mode_(is_popup_mode) {
  set_suppress_default_focus_handling();
  if (!is_popup_mode_) {
    views::FocusRing::Install(this);
    views::FocusRing::Get(this)->SetHasFocusPredicate(
        base::BindRepeating([](const View* view) {
          const auto* v = views::AsViewClass<LocationBarView>(view);
          CHECK(v);
          // Show focus ring when the Omnibox is visibly focused and the popup
          // is closed.
          return v->omnibox_view_->model()->is_caret_visible() &&
                 !v->GetOmniboxPopupView()->IsOpen();
        }));
    views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
    views::InstallPillHighlightPathGenerator(this);

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
    if (features::IsOsLevelGeolocationPermissionSupportEnabled()) {
      geolocation_permission_observation_.Observe(
          device::GeolocationSystemPermissionManager::GetInstance());
    }
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  }
#if BUILDFLAG(IS_MAC)
  app_shim_observation_ =
      AppShimRegistry::Get()->RegisterAppChangedCallback(base::BindRepeating(
          &LocationBarView::OnAppShimChanged, base::Unretained(this)));
#endif
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
}

LocationBarView::~LocationBarView() = default;

void LocationBarView::Init() {
  // We need to be in a Widget, otherwise GetNativeTheme() may change and we're
  // not prepared for that.
  DCHECK(GetWidget());

  // Note that children with layers are *not* clipped, because focus rings have
  // to draw outside the parent's bounds.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  if (base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators)) {
    permission_dashboard_view_ =
        AddChildViewAt(std::make_unique<PermissionDashboardView>(), 0);

    permission_dashboard_controller_ =
        std::make_unique<PermissionDashboardController>(
            this, permission_dashboard_view_);
  } else {
    chip_controller_ = std::make_unique<ChipController>(
        this, AddChildViewAt(std::make_unique<PermissionChipView>(
                                 PermissionChipView::PressedCallback()),
                             0));
  }

  const auto& typography_provider = views::TypographyProvider::Get();
  const gfx::FontList& font_list = typography_provider.GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);

  const gfx::FontList& omnibox_chip_font_list = typography_provider.GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_BODY_4_EMPHASIS);
  const gfx::FontList& page_action_font_list = typography_provider.GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_BODY_3_EMPHASIS);

  auto location_icon_view =
      std::make_unique<LocationIconView>(omnibox_chip_font_list, this, this);
  location_icon_view->set_drag_controller(this);
  location_icon_view_ = AddChildView(std::move(location_icon_view));

  // Initialize the Omnibox view.
  auto omnibox_view = std::make_unique<OmniboxViewViews>(
      std::make_unique<ChromeOmniboxClient>(
          /*location_bar=*/this, browser_, profile_),
      is_popup_mode_,
      /*location_bar_view=*/this, font_list);
  omnibox_view_ = AddChildView(std::move(omnibox_view));
  omnibox_view_->Init();
  // LocationBarView directs mouse button events from
  // |omnibox_additional_text_view_| to |omnibox_view_| so that e.g., clicking
  // the former will focus the latter. In order to receive |ShowContextMenu()|
  // requests, LocationBarView must have a context menu controller.
  set_context_menu_controller(omnibox_view_->context_menu_controller());

  RefreshBackground();

  // Initialize the IME autocomplete labels which are visible only when IME is
  // turned on.  Use the same font with the omnibox and highlighted background.
  const auto* const color_provider = GetColorProvider();
  auto CreateImeAutocompletionLabel =
      [&](gfx::HorizontalAlignment horizontal_alignment) {
        auto label = std::make_unique<views::Label>(
            std::u16string(), views::Label::CustomFont{font_list});
        label->SetHorizontalAlignment(horizontal_alignment);
        label->SetElideBehavior(gfx::NO_ELIDE);
        label->SetAutoColorReadabilityEnabled(false);
        label->SetBackground(views::CreateSolidBackground(
            color_provider->GetColor(kColorLocationBarBackground)));
        label->SetEnabledColor(color_provider->GetColor(kColorOmniboxText));
        label->SetVisible(false);
        return label;
      };

  if (OmniboxPrefixRichAutocompletionEnabled()) {
    ime_prefix_autocomplete_view_ =
        AddChildView(CreateImeAutocompletionLabel(gfx::ALIGN_RIGHT));
  }
  ime_inline_autocomplete_view_ =
      AddChildView(CreateImeAutocompletionLabel(gfx::ALIGN_LEFT));

  // Initiate the Omnibox additional-text label.
  if (OmniboxFieldTrial::RichAutocompletionShowAdditionalText()) {
    auto omnibox_additional_text_view = std::make_unique<views::Label>(
        std::u16string(), CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);
    omnibox_additional_text_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    omnibox_additional_text_view->SetVisible(false);
    omnibox_additional_text_view_ =
        AddChildView(std::move(omnibox_additional_text_view));
    omnibox_additional_text_view_->SetEnabledColorId(kColorOmniboxResultsUrl);
  }

  selected_keyword_view_ = AddChildView(
      std::make_unique<SelectedKeywordView>(this, profile_, font_list));

  if (browser_ && apps::features::ShouldShowLinkCapturingUX()) {
    intent_chip_ =
        AddChildView(std::make_unique<IntentChipButton>(browser_, this));
  }

  SkColor icon_color = color_provider->GetColor(kColorOmniboxResultsIcon);

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), this, this, page_action_font_list);
    image_view->SetIconColor(icon_color);
    image_view->SetVisible(false);
    content_setting_views_.push_back(AddChildView(std::move(image_view)));
  }

  PageActionIconParams params;
  // |browser_| may be null when LocationBarView is used for non-Browser windows
  // such as PresentationReceiverWindowView, which do not support page actions.
  if (browser_) {
    // Page action icons that participate in label animations should be added
    // first so that they appear on the left side of the icon container.
    // TODO(crbug.com/40835681): Improve the ordering heuristics for page action
    // icons and determine a way to handle simultaneous icon animations.
    if (base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
      params.types_enabled.push_back(
          PageActionIconType::kProductSpecifications);
    }
    params.types_enabled.push_back(PageActionIconType::kDiscounts);
    params.types_enabled.push_back(PageActionIconType::kPriceInsights);
    params.types_enabled.push_back(PageActionIconType::kPriceTracking);

    if (side_search::IsEnabledForBrowser(browser_)) {
      params.types_enabled.push_back(PageActionIconType::kSideSearch);
    }

    params.types_enabled.push_back(PageActionIconType::kClickToCall);
    params.types_enabled.push_back(PageActionIconType::kSmsRemoteFetcher);
    params.types_enabled.push_back(PageActionIconType::kManagePasswords);
    if (!apps::features::ShouldShowLinkCapturingUX()) {
      params.types_enabled.push_back(PageActionIconType::kIntentPicker);
    }
    params.types_enabled.push_back(PageActionIconType::kPwaInstall);
    params.types_enabled.push_back(PageActionIconType::kFind);
    params.types_enabled.push_back(PageActionIconType::kTranslate);
    params.types_enabled.push_back(PageActionIconType::kZoom);
    params.types_enabled.push_back(PageActionIconType::kFileSystemAccess);

    params.types_enabled.push_back(PageActionIconType::kCookieControls);
    params.types_enabled.push_back(
        PageActionIconType::kPaymentsOfferNotification);
    params.types_enabled.push_back(PageActionIconType::kMemorySaver);
  }
  params.types_enabled.push_back(PageActionIconType::kSaveCard);
  params.types_enabled.push_back(PageActionIconType::kSaveIban);
  params.types_enabled.push_back(PageActionIconType::kLocalCardMigration);
  params.types_enabled.push_back(
      PageActionIconType::kVirtualCardManualFallback);
  params.types_enabled.push_back(PageActionIconType::kVirtualCardEnroll);
  params.types_enabled.push_back(PageActionIconType::kMandatoryReauth);

  // TODO(crbug.com/40164487): Place this in the proper order upon having final
  // mocks.
  params.types_enabled.push_back(PageActionIconType::kAutofillAddress);

  if (browser_ && lens::features::IsOmniboxEntryPointEnabled()) {
    // The persistent compact entrypoint should be positioned directly before
    // the star icon and the prominent expanding entrypoint should be
    // positioned in the leading position.
    if (lens::features::IsOmniboxEntrypointAlwaysVisible()) {
      params.types_enabled.push_back(PageActionIconType::kLensOverlay);
    } else {
      params.types_enabled.insert(params.types_enabled.begin(),
                                  PageActionIconType::kLensOverlay);
    }
  }

  if (browser_ && !is_popup_mode_) {
    params.types_enabled.push_back(PageActionIconType::kBookmarkStar);
  }

  params.icon_color = color_provider->GetColor(kColorPageActionIcon);
  params.between_icon_spacing = 8;
  params.font_list = &page_action_font_list;
  params.browser = browser_;
  params.command_updater = command_updater();
  params.icon_label_bubble_delegate = this;
  params.page_action_icon_delegate = this;
  page_action_icon_container_ =
      AddChildView(std::make_unique<PageActionIconContainerView>(params));
  page_action_icon_controller_ = page_action_icon_container_->controller();

  auto clear_all_button = views::CreateVectorImageButton(base::BindRepeating(
      static_cast<void (OmniboxView::*)(const std::u16string&)>(
          &OmniboxView::SetUserText),
      base::Unretained(omnibox_view_), std::u16string()));
  clear_all_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_CLEAR_ALL));
  clear_all_button_ = AddChildView(std::move(clear_all_button));
  RefreshClearAllButtonIcon();

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(nullptr);

  hover_animation_.SetSlideDuration(base::Milliseconds(200));

  is_initialized_ = true;
}

bool LocationBarView::IsInitialized() const {
  return is_initialized_;
}

int LocationBarView::GetBorderRadius() const {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, size());
}

std::unique_ptr<views::Background> LocationBarView::CreateRoundRectBackground(
    SkColor background_color,
    SkColor stroke_color,
    SkBlendMode blend_mode,
    bool antialias,
    bool should_border_scale) const {
  const int radius = GetBorderRadius();
  auto painter =
      stroke_color == SK_ColorTRANSPARENT
          ? views::Painter::CreateSolidRoundRectPainter(
                background_color, radius, gfx::Insets(), blend_mode, antialias)
          : views::Painter::CreateRoundRectWith1PxBorderPainter(
                background_color, stroke_color, radius, blend_mode, antialias,
                should_border_scale);
  std::unique_ptr<views::Background> background =
      CreateBackgroundFromPainter(std::move(painter));
  background->SetNativeControlColor(background_color);
  return background;
}

gfx::Point LocationBarView::GetOmniboxViewOrigin() const {
  gfx::Point origin(omnibox_view_->origin());
  origin.set_x(GetMirroredXInView(origin.x()));
  views::View::ConvertPointToScreen(this, &origin);
  return origin;
}

void LocationBarView::SetImePrefixAutocompletion(const std::u16string& text) {
  DCHECK(OmniboxPrefixRichAutocompletionEnabled() || text.empty());
  if (OmniboxPrefixRichAutocompletionEnabled())
    SetOmniboxAdjacentText(ime_prefix_autocomplete_view_, text);
}

std::u16string LocationBarView::GetImePrefixAutocompletion() const {
  return OmniboxPrefixRichAutocompletionEnabled()
             ? ime_prefix_autocomplete_view_->GetText()
             : u"";
}

void LocationBarView::SetImeInlineAutocompletion(const std::u16string& text) {
  SetOmniboxAdjacentText(ime_inline_autocomplete_view_, text);
}

std::u16string LocationBarView::GetImeInlineAutocompletion() const {
  return ime_inline_autocomplete_view_->GetText();
}

void LocationBarView::SetOmniboxAdditionalText(const std::u16string& text) {
  DCHECK(OmniboxFieldTrial::IsRichAutocompletionEnabled() || text.empty());
  if (!OmniboxFieldTrial::RichAutocompletionShowAdditionalText())
    return;

  std::u16string adjusted_text;
  if (!text.empty()) {
    const int message_id =
        OmniboxFieldTrial::kRichAutocompletionAdditionalTextWithParenthesis
                .Get()
            ? IDS_OMNIBOX_ADDITIONAL_TEXT_PARENTHESIS_TEMPLATE
            : IDS_OMNIBOX_ADDITIONAL_TEXT_DASH_TEMPLATE;
    adjusted_text = text;
    base::i18n::AdjustStringForLocaleDirection(&adjusted_text);
    adjusted_text = l10n_util::GetStringFUTF16(message_id, u"", adjusted_text);
  }
  SetOmniboxAdjacentText(omnibox_additional_text_view_, adjusted_text);
}

std::u16string LocationBarView::GetOmniboxAdditionalText() const {
  return OmniboxFieldTrial::RichAutocompletionShowAdditionalText()
             ? omnibox_additional_text_view_->GetText()
             : u"";
}

void LocationBarView::SetOmniboxAdjacentText(views::Label* label,
                                             const std::u16string& text) {
  if (text == label->GetText())
    return;
  label->SetText(text);
  label->SetVisible(!text.empty());
  OnPropertyChanged(&label, views::kPropertyEffectsLayout);
}

void LocationBarView::SelectAll() {
  omnibox_view_->SelectAll(true);
}

void LocationBarView::FocusLocation(bool is_user_initiated) {
  omnibox_view_->SetFocus(is_user_initiated);
}

void LocationBarView::Revert() {
  omnibox_view_->RevertAll();
}

OmniboxView* LocationBarView::GetOmniboxView() {
  return omnibox_view_;
}

void LocationBarView::AddedToWidget() {
  if (lens::features::IsOmniboxEntryPointEnabled() && browser_ &&
      GetFocusManager()) {
    CHECK(!focus_manager_);
    focus_manager_ = GetFocusManager();
    focus_manager_->AddFocusChangeListener(this);
  }
}

void LocationBarView::RemovedFromWidget() {
  // No-op unless registered (see above).
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
  }
}

void LocationBarView::OnWillChangeFocus(views::View* before, views::View* now) {
}

void LocationBarView::OnDidChangeFocus(views::View* before, views::View* now) {
  // This is very blunt. There's a page action (LensOverlayPageActionView) whose
  // visibility state depends on whether focus is within the location bar or
  // not. Maybe that dependency should be better understood rather than "refresh
  // all page actions if focus changes". For now for expediency we update the
  // page actions when focus changes under the assumption that this in practice
  // isn't likely to be janky (or we already have a problem here).
  //
  // TODO(pbos): We should move focus listening to the LensOverlayPageActionView
  // instead and have that invoke LocationBarView::RefreshPageActionIconViews
  // instead. That would make sure that its dependency on FocusManager is
  // explicit and also make sure that the corresponding focus-listening code
  // would get cleaned up if no page action needs it. It would also be great if
  // views supported declaring interest in whether focus is inside / outside a
  // View hierarchy rather than monitoring any focus changes.
  //
  // We post a task instead of synchronously updating the page actions due to a
  // bug where navigation triggers dialog closure which triggers a focus change
  // which calls here. If we directly call UpdateAll() here then
  // CookieControlsIconView will try to prompt a RenderFrameHost::IsSandboxed()
  // but the RenderFrameHost hasn't yet been updated to be queryable for
  // IsSandboxed() during this stack so we crash. By posting a task we make sure
  // the RenderFrameHost is not in the middle of updating its own state.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LocationBarView::RefreshPageActionIconViews,
                                weak_factory_.GetWeakPtr()));
}

bool LocationBarView::HasFocus() const {
  return omnibox_view_ && omnibox_view_->model()->has_focus();
}

gfx::Size LocationBarView::GetMinimumSize() const {
  const int height = GetLayoutConstant(LOCATION_BAR_HEIGHT);
  if (!IsInitialized())
    return gfx::Size(0, height);

  const int inset_width = GetInsets().width();
  const int padding = GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING);
  const int leading_width = GetMinimumLeadingWidth();
  const int omnibox_width = omnibox_view_->GetMinimumSize().width();
  const int trailing_width = GetMinimumTrailingWidth();

  // The minimum width of the location bar is defined to be the greater of the
  // minimum width of the location text field and the space required for the
  // other child views. This ensures that the location bar can shrink
  // significantly when the browser window is small and the toolbar is crowded
  // but also keeps the minimum size relatively stable when the number and size
  // of location bar child views changes (i.e. when there are multiple status
  // indicators and a large security chip vs. just the location text).
  int alt_width = leading_width + padding + trailing_width;
  int width = inset_width + std::max(omnibox_width, alt_width);

  return gfx::Size(width, height);
}

gfx::Size LocationBarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int height = GetLayoutConstant(LOCATION_BAR_HEIGHT);
  if (!IsInitialized())
    return gfx::Size(0, height);

  const int inset_width = GetInsets().width();
  const int padding = GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING);
  const int leading_width = GetMinimumLeadingWidth();
  const int omnibox_width = omnibox_view_->GetMinimumSize().width();
  const int trailing_width = GetMinimumTrailingWidth();

  // The preferred size (unlike the minimum size) of the location bar is roughly
  // the combined size of all child views including the omnibox/location field.
  // While the location bar can scale down to its minimum size, it will continue
  // to displace lower-priority views such as visible extensions if it cannot
  // achieve its preferred size.
  //
  // It might be useful to track the preferred size of the location bar to see
  // how much visual clutter users are experiencing on a regular basis,
  // especially as we add more indicators to the bar.
  int width = inset_width + omnibox_width;
  if (leading_width > 0)
    width += leading_width + padding;
  if (trailing_width > 0)
    width += trailing_width + padding;

  return gfx::Size(width, height);
}

void LocationBarView::OnKeywordFaviconFetched(const gfx::Image& icon) {
  DCHECK(!icon.IsEmpty());
  selected_keyword_view_->SetCustomImage(icon);
}

void LocationBarView::Layout(PassKey) {
  if (!IsInitialized())
    return;

  selected_keyword_view_->SetVisible(false);

  const int trailing_decoration_inner_padding =
      GetLayoutConstant(LOCATION_BAR_TRAILING_DECORATION_INNER_PADDING);

  // The text should be indented only if these are all true:
  //  - The popup is open.
  //  - The location icon view does *not* have a label.
  //  - The selected keyword view is *not* shown.
  //
  // In most cases, we only care that the popup is open, in which case we
  // indent to align with the text in the popup. But there's two edge cases:
  //  - If there is text in the location icon view (which can happen with zero
  //    suggest, which continues to show security or EV cert text at the same
  //    time as the popup is open), the text in the omnibox can't align with
  //    the text of the suggestions, so the indent just moves the text for no
  //    apparent reason.
  //  - If there is a selected keyword label (i.e. "Search Google") shown, we
  //    already indent this label to align with the suggestions text, so
  //    further indenting the textfield just moves the text for no apparent
  //    reason.
  //
  // TODO(jdonnelly): The better solution may be to remove the location icon
  // text when zero suggest triggers.
  const bool should_indent = GetOmniboxPopupView()->IsOpen() &&
                             !location_icon_view_->ShouldShowLabel() &&
                             !ShouldShowKeywordBubble();

  const bool show_overriding_permission_chip =
      base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators)
          ? permission_dashboard_view_->GetVisible() &&
                !ShouldShowKeywordBubble()
          : chip_controller_->chip()->GetVisible() &&
                !ShouldShowKeywordBubble();

  // There are 2 CR23 features that impact location bar layout. Make sure layout
  // is correct when neither, either, or both are enabled. Touch UI, whether the
  // popup is open (see `should_indent` comment above), whether a keyword is
  // selected, and whether the permission chip is shown also affect layout.
  // TODO(manukh): The permutation space is pretty large, and we don't have
  //   mocks for every single case. So we do something that looks right for now,
  //   and can iron out the details post CR23. E.g. this probably shifts some
  //   touch UI layout even when the CR23 features are disabled.
  // TODO(manukh): Once we decide what to launch, we can keep just one of these,
  //   and move it to layout_constants.cc.
  // The padding between the left edges of the location bar and the LHS icon
  // (e.g. the page info icon, the google G icon, the selected suggestion icon,
  // etc)
  int icon_left = 5;
  // The padding between the LHS icon and the text.
  int text_left = 8;
  // Indentation to match the suggestion icons & texts.
  int icon_indent = 7;
  int text_indent = 6;
  // Indentation to match the suggestion icons & texts when in keyword mode.
  int icon_keyword_indent = 9;
  int text_keyword_indent = -9;
  // Indentation add padding when the permission chip is visible and replacing
  // the LHS icon.
  int text_overriding_permission_chip_indent = 0;
  if (should_indent) {
    icon_left += icon_indent;
    text_left += text_indent;
  }
  if (ShouldShowKeywordBubble()) {
    icon_left += icon_keyword_indent;
    text_left += text_keyword_indent;
  }
  if (show_overriding_permission_chip)
    text_left += text_overriding_permission_chip_indent;

  LocationBarLayout leading_decorations(LocationBarLayout::Position::kLeftEdge,
                                        text_left);
  LocationBarLayout trailing_decorations(
      LocationBarLayout::Position::kRightEdge,
      trailing_decoration_inner_padding);

  const std::u16string keyword(omnibox_view_->model()->keyword());
  // In some cases (e.g. fullscreen mode) we may have 0 height.  We still want
  // to position our child views in this case, because other things may be
  // positioned relative to them (e.g. the "bookmark added" bubble if the user
  // hits ctrl-d).
  const int vertical_padding =
      GetLayoutConstant(LOCATION_BAR_PAGE_INFO_ICON_VERTICAL_PADDING);
  const int trailing_decorations_edge_padding =
      GetLayoutConstant(LOCATION_BAR_TRAILING_DECORATION_EDGE_PADDING);

  const int location_height = std::max(height() - (vertical_padding * 2), 0);
  // The largest fraction of the omnibox that can be taken by the EV or search
  // label/chip.
  const double kLeadingDecorationMaxFraction = 0.5;

  if (show_overriding_permission_chip) {
    if (base::FeatureList::IsEnabled(
            content_settings::features::kLeftHandSideActivityIndicators)) {
      leading_decorations.AddDecoration(vertical_padding, location_height,
                                        false, 0, /*intra_item_padding=*/0,
                                        icon_left, permission_dashboard_view_);
    } else {
      leading_decorations.AddDecoration(vertical_padding, location_height,
                                        false, 0, /*intra_item_padding=*/0,
                                        icon_left, chip_controller_->chip());
    }
  }

  if (ShouldShowKeywordBubble()) {
    location_icon_view_->SetVisible(false);
    leading_decorations.AddDecoration(
        vertical_padding, location_height, false, kLeadingDecorationMaxFraction,
        /*intra_item_padding=*/0, icon_left, selected_keyword_view_);
    if (selected_keyword_view_->GetKeyword() != keyword) {
      selected_keyword_view_->SetKeyword(keyword);
      const TemplateURL* template_url =
          TemplateURLServiceFactory::GetForProfile(profile_)
              ->GetTemplateURLForKeyword(keyword);
      gfx::Image image;
      if (template_url &&
          (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)) {
        image = extensions::OmniboxAPI::Get(profile_)->GetOmniboxIcon(
            template_url->GetExtensionId());
      }
      selected_keyword_view_->SetCustomImage(image);
    }
  } else if (location_icon_view_->GetShowText() &&
             !ShouldChipOverrideLocationIcon()) {
    location_icon_view_->SetVisible(true);
    leading_decorations.AddDecoration(
        vertical_padding, location_height, false, kLeadingDecorationMaxFraction,
        /*intra_item_padding=*/0, icon_left, location_icon_view_);
  } else if (!ShouldChipOverrideLocationIcon()) {
    location_icon_view_->SetVisible(true);
    leading_decorations.AddDecoration(vertical_padding, location_height, false,
                                      0, /*intra_item_padding=*/0, icon_left,
                                      location_icon_view_);
  } else {
    location_icon_view_->SetVisible(false);
  }

  auto add_trailing_decoration = [&](View* view, int intra_item_padding) {
    if (view->GetVisible()) {
      trailing_decorations.AddDecoration(
          vertical_padding, location_height, /*auto_collapse=*/false,
          /*max_fraction=*/0, intra_item_padding,
          trailing_decorations_edge_padding, view);
    }
  };

  add_trailing_decoration(page_action_icon_container_,
                          /*intra_item_padding=*/0);
  for (ContentSettingImageView* view : base::Reversed(content_setting_views_)) {
    int intra_item_padding = kContentSettingIntraItemPadding;
    add_trailing_decoration(view, intra_item_padding);
  }

  if (intent_chip_) {
    int intra_item_padding = kIntentChipIntraItemPadding;
    add_trailing_decoration(intent_chip_, intra_item_padding);
  }

  add_trailing_decoration(clear_all_button_, /*intra_item_padding=*/0);

  // Perform layout.
  int entry_width = width();

  // Use the unelided omnibox width as the `reserved_width` so preferred size
  // calculations of decorations are calculated to maximize this constraint.
  // TODO(crbug.com/350541615): This can be removed once current non-resizable
  // decorations are updated to support LocationBayLayout::auto_collapse.
  const int inset_width = GetInsets().width();
  const int padding = GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING);
  const int unelided_omnibox_width = omnibox_view_->GetUnelidedTextWidth();
  const int reserved_width = unelided_omnibox_width + inset_width + padding * 2;

  leading_decorations.LayoutPass1(&entry_width, reserved_width);
  trailing_decorations.LayoutPass1(&entry_width, reserved_width);
  leading_decorations.LayoutPass2(&entry_width);
  trailing_decorations.LayoutPass2(&entry_width);

  // Compute widths needed for location bar.
  int location_needed_width = omnibox_view_->GetUnelidedTextWidth();

  int available_width = entry_width - location_needed_width;
  // The bounds must be wide enough for all the decorations to fit, so if
  // |entry_width| is negative, enlarge by the necessary extra space.
  gfx::Rect location_bounds(0, vertical_padding,
                            std::max(width(), width() - entry_width),
                            location_height);
  leading_decorations.LayoutPass3(&location_bounds, &available_width);
  trailing_decorations.LayoutPass3(&location_bounds, &available_width);

  // |omnibox_view_| has an opaque background, so ensure it doesn't paint atop
  // the rounded ends.
  location_bounds.Intersect(GetLocalBoundsWithoutEndcaps());

  if ((!OmniboxPrefixRichAutocompletionEnabled() ||
       !ime_prefix_autocomplete_view_->GetVisible()) &&
      !ime_inline_autocomplete_view_->GetVisible() &&
      (!OmniboxFieldTrial::RichAutocompletionShowAdditionalText() ||
       !omnibox_additional_text_view_->GetVisible())) {
    // Short circuit the below logic when the additional views aren't visible.
    // This is necessary as resizing the omnibox can throw off it's scroll,
    // i.e., which chars are visible when its text overflows its width.
    // TODO(manukh): The omnibox 1) sets its text, then 2) sets its scroll, and
    //  lastly 3) asks the location bar to update its layout. Step (3) may
    //  resize the omnibox; doing so after (2) can dirty the scroll. This
    //  workaround handles most cases by avoiding omnibox resizing when possible
    //  but it's not foolproof. E.g., accepting IME autocompletion will result
    //  in an incorrect scroll until the next update. Look into doing (3) before
    //  (2) to more robustly handle these edge cases.
    omnibox_view_->SetBoundsRect(location_bounds);

  } else {
    // A helper to allocate the remaining location bar width preferring calls in
    // the order they're made; e.g. if there's 100px remaining, and
    // `reserve_width()` is invoked with '70' and '70', the first caller will
    // receive 70px and the 2nd caller will receive 30px; subsequent callers
    // will receive 0px.
    int remaining_width = location_bounds.width();
    const auto reserve_width = [&](int desired_width) {
      int width = std::min(desired_width, remaining_width);
      remaining_width -= width;
      return width;
    };
    // A helper to request from `reserve_width()` the width needed for `label`.
    const auto reserve_label_width = [&](views::Label* label) {
      if (!label || !label->GetVisible())
        return 0;
      int text_width =
          gfx::GetStringWidth(label->GetText(), label->font_list());
      return reserve_width(text_width + label->GetInsets().width());
    };

    // Distribute `remaining_width` among the 4 views.
    int omnibox_width = reserve_width(location_needed_width);
    int ime_inline_autocomplete_width =
        reserve_label_width(ime_inline_autocomplete_view_);
    int ime_prefix_autocomplete_width =
        reserve_label_width(ime_prefix_autocomplete_view_);
    int omnibox_additional_text_width =
        reserve_label_width(omnibox_additional_text_view_);

    // A helper to position `view` to the right of the previous positioned
    // `view`.
    int current_x = location_bounds.x();
    const auto position_view = [&](views::View* view, int width) {
      if (!view || !view->GetVisible())
        return;
      view->SetBounds(current_x, location_bounds.y(), width,
                      location_bounds.height());
      current_x = view->bounds().right();
    };

    // Position the 4 views
    position_view(ime_prefix_autocomplete_view_, ime_prefix_autocomplete_width);
    position_view(omnibox_view_, omnibox_width);
    position_view(ime_inline_autocomplete_view_, ime_inline_autocomplete_width);
    position_view(omnibox_additional_text_view_, omnibox_additional_text_width);
  }

  LayoutSuperclass<View>(this);
}

void LocationBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // ToolbarView::Init() adds |this| to the view hierarchy before initializing,
  // which will trigger an early theme change.
  if (!IsInitialized())
    return;

  const SkColor icon_color = GetColorProvider()->GetColor(kColorPageActionIcon);
  page_action_icon_controller_->SetIconColor(icon_color);
  for (ContentSettingImageView* image_view : content_setting_views_) {
    image_view->SetIconColor(icon_color);
  }

  RefreshBackground();
  RefreshClearAllButtonIcon();
}

void LocationBarView::ChildPreferredSizeChanged(views::View* child) {
  InvalidateLayout();
  SchedulePaint();
}

bool LocationBarView::HasSecurityStateChanged() {
  return location_icon_view_->HasSecurityStateChanged();
}

void LocationBarView::Update(WebContents* contents) {
  if (contents)
    page_action_icon_controller_->UpdateWebContents(contents);

  RefreshContentSettingViews();

  RefreshPageActionIconViews();
  location_icon_view_->Update(/*suppress_animations=*/contents,
                              omnibox_view_->model()->PopupIsOpen());

  if (intent_chip_)
    intent_chip_->Update();

  if (contents)
    omnibox_view_->OnTabChanged(contents);
  else
    omnibox_view_->Update();

  OnChanged();  // NOTE: Triggers layout.

  // A permission prompt may be suspended due to an invalid state (empty or
  // editing location bar). Restore the suspended prompt if possible.
  if (contents && !IsEditingOrEmpty()) {
    auto* permission_request_manager =
        permissions::PermissionRequestManager::FromWebContents(contents);
    if (permission_request_manager->CanRestorePrompt())
      permission_request_manager->RestorePrompt();
  }
}

void LocationBarView::ResetTabState(WebContents* contents) {
  omnibox_view_->ResetTabState(contents);
}

bool LocationBarView::ActivateFirstInactiveBubbleForAccessibility() {
  return page_action_icon_controller_
      ->ActivateFirstInactiveBubbleForAccessibility();
}

ChipController* LocationBarView::GetChipController() {
  if (base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators)) {
    return permission_dashboard_controller_->request_chip_controller();
  }

  return chip_controller_.get();
}

void LocationBarView::UpdateWithoutTabRestore() {
  Update(nullptr);
}

LocationBarModel* LocationBarView::GetLocationBarModel() {
  return delegate_->GetLocationBarModel();
}

WebContents* LocationBarView::GetWebContents() {
  return delegate_->GetWebContents();
}

SkColor LocationBarView::GetIconLabelBubbleSurroundingForegroundColor() const {
  // If keyword mode is active, then override the "surrounding foreground color"
  // to ensure that the keyword mode separator has a distinct color. Otherwise,
  // fall back to the usual omnibox text color to ensure UI consistency.
  // In either case, all IconLabelBubbleViews situated within the location bar
  // will inherit the selected "surrounding foreground color".
  const auto color_id = ShouldShowKeywordBubble()
                            ? kColorOmniboxKeywordSeparator
                            : kColorPageActionIcon;
  return GetColorProvider()->GetColor(color_id);
}

SkAlpha LocationBarView::GetIconLabelBubbleSeparatorAlpha() const {
  return 0xFF;
}

SkColor LocationBarView::GetIconLabelBubbleBackgroundColor() const {
  return GetColorProvider()->GetColor(kColorLocationBarBackground);
}

bool LocationBarView::ShouldHideContentSettingImage() {
  return ShouldHidePageActionIcons();
}

content::WebContents* LocationBarView::GetContentSettingWebContents() {
  return GetWebContents();
}

ContentSettingBubbleModelDelegate*
LocationBarView::GetContentSettingBubbleModelDelegate() {
  return delegate_->GetContentSettingBubbleModelDelegate();
}

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
void LocationBarView::OnSystemPermissionUpdated(
    device::LocationSystemPermissionStatus new_status) {
  UpdateContentSettingsIcons();
}
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

WebContents* LocationBarView::GetWebContentsForPageActionIconView() {
  return GetWebContents();
}

bool LocationBarView::ShouldHidePageActionIcons() const {
  if (!omnibox_view_)
    return false;

  // When the user is typing in the omnibox, the page action icons are no longer
  // associated with the current omnibox text, so hide them.
  if (omnibox_view_->model()->user_input_in_progress())
    return true;

  // Also hide them if the popup is open for any other reason, e.g. ZeroSuggest.
  // The page action icons are not relevant to the displayed suggestions.
  return omnibox_view_->model()->PopupIsOpen();
}

bool LocationBarView::ShouldHidePageActionIcon(
    PageActionIconView* icon_view) const {
  if (ShouldHidePageActionIcons()) {
    return true;
  }
  if (features::IsToolbarPinningEnabled() && browser_) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    if (browser_view) {
      auto* pinned_toolbar_actions_container =
          browser_view->toolbar()->pinned_toolbar_actions_container();
      return pinned_toolbar_actions_container &&
             pinned_toolbar_actions_container->IsActionPinnedOrPoppedOut(
                 icon_view->action_id().value());
    }
  }
  return false;
}

// static
bool LocationBarView::IsVirtualKeyboardVisible(views::Widget* widget) {
  if (auto* input_method = widget->GetInputMethod()) {
    auto* keyboard = input_method->GetVirtualKeyboardController();
    return keyboard && keyboard->IsKeyboardVisible();
  }
  return false;
}

// static
int LocationBarView::GetAvailableTextHeight() {
  return std::max(0, GetLayoutConstant(LOCATION_BAR_HEIGHT) -
                         2 * GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING));
}

// static
int LocationBarView::GetAvailableDecorationTextHeight() {
  const int bubble_padding =
      GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING) +
      GetLayoutConstant(LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING);
  return std::max(
      0, LocationBarView::GetAvailableTextHeight() - (bubble_padding * 2));
}

int LocationBarView::GetMinimumLeadingWidth() const {
  // If the keyword bubble is showing, the view can collapse completely.
  if (ShouldShowKeywordBubble())
    return 0;

  if (location_icon_view_->GetShowText())
    return location_icon_view_->GetMinimumLabelTextWidth();

  return GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
         location_icon_view_->GetMinimumSize().width();
}

int LocationBarView::GetMinimumTrailingWidth() const {
  int trailing_width = IncrementalMinimumWidth(page_action_icon_container_);

  for (ContentSettingImageView* content_setting_view : content_setting_views_) {
    trailing_width += IncrementalMinimumWidth(content_setting_view);
  }

  return trailing_width;
}

gfx::Rect LocationBarView::GetLocalBoundsWithoutEndcaps() const {
  const int border_radius = height() / 2;
  gfx::Rect bounds_without_endcaps(GetLocalBounds());
  bounds_without_endcaps.Inset(gfx::Insets::VH(0, border_radius));
  return bounds_without_endcaps;
}

void LocationBarView::RefreshBackground() {
  const double opacity = hover_animation_.GetCurrentValue();
  const bool is_caret_visible = omnibox_view_->model()->is_caret_visible();
  const bool input_in_progress =
      omnibox_view_->model()->user_input_in_progress();
  const bool high_contrast = GetNativeTheme()->UserHasContrastPreference();

  const auto* const color_provider = GetColorProvider();
  SkColor normal = color_provider->GetColor(kColorLocationBarBackground);
  SkColor hovered =
      color_provider->GetColor(kColorLocationBarBackgroundHovered);

  SkColor background_color =
      gfx::Tween::ColorValueBetween(opacity, normal, hovered);
  if (is_caret_visible) {
    // Match the background color to the popup if the Omnibox is visibly
    // focused.
    background_color = color_provider->GetColor(kColorOmniboxResultsBackground);
  } else if (input_in_progress && !high_contrast) {
    // Under CR23 guidelines, if the Omnibox is unfocused, but still contains
    // in-progress user input, the background color matches the popup (unless
    // high-contrast mode is enabled).
    normal = color_provider->GetColor(kColorOmniboxResultsBackground);
    hovered = color_provider->GetColor(kColorOmniboxResultsBackgroundHovered);
    background_color = gfx::Tween::ColorValueBetween(opacity, normal, hovered);
  }

  SkColor border_color = SK_ColorTRANSPARENT;
  if (high_contrast) {
    // High contrast schemes get a border stroke even on a rounded omnibox.
    border_color =
        is_caret_visible
            ? color_provider->GetColor(kColorOmniboxResultsBackground)
            : color_provider->GetColor(kColorLocationBarBorder);
  } else if (!is_caret_visible && input_in_progress) {
    // Under CR23 guidelines, if the (regular contrast) Omnibox is unfocused,
    // but still contains in-progress user input, a unique border color will be
    // applied.
    border_color = color_provider->GetColor(kColorLocationBarBorderOnMismatch);
  }

  if (is_popup_mode_) {
    SetBackground(views::CreateSolidBackground(background_color));
  } else {
    SetBackground(CreateRoundRectBackground(
        background_color, border_color, /*blend_mode=*/SkBlendMode::kSrcOver,
        /*antialias=*/true, /*should_border_scale=*/true));
  }

  // Keep the views::Textfield in sync. It needs an opaque background to
  // correctly enable subpixel AA.
  omnibox_view_->SetBackgroundColor(background_color);

  // The divider between indicators and request chips should have the same color
  // as the omnibox.
  if (base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators)) {
    permission_dashboard_view_->SetDividerBackgroundColor(background_color);
  }

  SchedulePaint();
}

bool LocationBarView::RefreshContentSettingViews() {
  if (web_app::AppBrowserController::IsWebApp(browser_)) {
    // For web apps, the location bar is normally hidden and icons appear in
    // the window frame instead.
    if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser_)) {
      browser_view->UpdateWebAppStatusIconsVisiblity();
    }
  }

  bool visibility_changed = false;
  for (ContentSettingImageView* v : content_setting_views_) {
    const bool was_visible = v->GetVisible();
    // The Left-Hand Side indicators currently supports only
    // `ImageType::MEDIASTREAM`.
    if (v->GetType() == ContentSettingImageModel::ImageType::MEDIASTREAM &&
        // WebApps do not support the Left-Hand Side indicators.
        !web_app::AppBrowserController::IsWebApp(browser_) &&
        base::FeatureList::IsEnabled(
            content_settings::features::kLeftHandSideActivityIndicators)) {
      visibility_changed |= permission_dashboard_controller()->Update(
          v->content_setting_image_model(), v->delegate());
    } else {
      v->Update();
      if (was_visible != v->GetVisible()) {
        visibility_changed = true;
      }
    }
  }
  return visibility_changed;
}

void LocationBarView::RefreshPageActionIconViews() {
  if (web_app::AppBrowserController::IsWebApp(browser_)) {
    // For web apps, the location bar is normally hidden and icons appear in
    // the window frame instead.
    if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser_)) {
      browser_view->UpdateWebAppStatusIconsVisiblity();
    }
  }

  page_action_icon_controller_->UpdateAll();
}

void LocationBarView::RefreshClearAllButtonIcon() {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  const gfx::VectorIcon& icon =
      touch_ui ? omnibox::kClearIcon : kTabCloseNormalIcon;
  const ui::ColorProvider* cp = GetColorProvider();
  SetImageFromVectorIconWithColor(
      clear_all_button_, icon,
      cp->GetColor(kColorLocationBarClearAllButtonIcon),
      cp->GetColor(kColorLocationBarClearAllButtonIconDisabled));
  clear_all_button_->SetBorder(views::CreateEmptyBorder(
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING)));
}

bool LocationBarView::ShouldShowKeywordBubble() const {
  return omnibox_view_->model()->is_keyword_selected();
}

OmniboxPopupView* LocationBarView::GetOmniboxPopupView() {
  return const_cast<OmniboxPopupView*>(
      std::as_const(*this).GetOmniboxPopupView());
}

const OmniboxPopupView* LocationBarView::GetOmniboxPopupView() const {
  DCHECK(omnibox_view_ && omnibox_view_->model());
  return omnibox_view_->model()->get_popup_view();
}

void LocationBarView::OnPageInfoBubbleClosed(
    views::Widget::ClosedReason closed_reason,
    bool reload_prompt) {
  // If we're closing the bubble because the user pressed ESC or because the
  // user clicked Close (rather than the user clicking directly on something
  // else), we should refocus the location bar. This lets the user tab into the
  // "You should reload this page" infobar rather than dumping them back out
  // into a stale webpage.
  if (!reload_prompt)
    return;
  if (closed_reason != views::Widget::ClosedReason::kEscKeyPressed &&
      closed_reason != views::Widget::ClosedReason::kCloseButtonClicked) {
    return;
  }

  FocusLocation(false);
}

void LocationBarView::FocusSearch() {
  // This is called by keyboard accelerator, so it's user-initiated.
  omnibox_view_->SetFocus(/*is_user_initiated=*/true);
  omnibox_view_->EnterKeywordModeForDefaultSearchProvider();
}

void LocationBarView::UpdateContentSettingsIcons() {
  if (RefreshContentSettingViews()) {
    // TODO(crbug.com/40648316): Remove Layout override and transition
    // LocationBarView to use a layout manager. Then when child view visibility
    // changes LocationBarView's layout will be automatically invalidated and
    // this InvalidateLayout() call can be removed.
    InvalidateLayout();
  }
}

void LocationBarView::SaveStateToContents(WebContents* contents) {
  omnibox_view_->SaveStateToTab(contents);
}

LocationBarTesting* LocationBarView::GetLocationBarForTesting() {
  return this;
}

bool LocationBarView::TestContentSettingImagePressed(size_t index) {
  if (index >= content_setting_views_.size())
    return false;

  views::View* image_view = content_setting_views_[index];
  if (!image_view->GetVisible())
    return false;

  image_view->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SPACE, ui::EF_NONE));
  image_view->OnKeyReleased(
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_SPACE, ui::EF_NONE));
  return true;
}

bool LocationBarView::IsContentSettingBubbleShowing(size_t index) {
  return index < content_setting_views_.size() &&
         content_setting_views_[index]->IsBubbleShowing();
}

void LocationBarView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  RefreshBackground();
}

bool LocationBarView::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void LocationBarView::OnVisibleBoundsChanged() {
  OmniboxPopupView* popup = GetOmniboxPopupView();
  if (popup->IsOpen())
    popup->UpdatePopupAppearance();
}

void LocationBarView::OnFocus() {
  // This is only called when the user explicitly focuses the location bar.
  // Renderer-initiated focuses go through the `FocusLocation()` call instead.
  omnibox_view_->SetFocus(/*is_user_initiated=*/true);
}

void LocationBarView::OnPaintBorder(gfx::Canvas* canvas) {
  if (!is_popup_mode_)
    return;  // The border is painted by our Background.

  gfx::Rect bounds(GetContentsBounds());
  const SkColor border_color =
      GetColorProvider()->GetColor(kColorLocationBarBorderOpaque);
  canvas->DrawLine(gfx::PointF(bounds.x(), bounds.y()),
                   gfx::PointF(bounds.right(), bounds.y()), border_color);
  canvas->DrawLine(gfx::PointF(bounds.x(), bounds.bottom() - 1),
                   gfx::PointF(bounds.right(), bounds.bottom() - 1),
                   border_color);
}

bool LocationBarView::OnMousePressed(const ui::MouseEvent& event) {
  return omnibox_view_->OnMousePressed(
      AdjustMouseEventLocationForOmniboxView(event));
}

bool LocationBarView::OnMouseDragged(const ui::MouseEvent& event) {
  return omnibox_view_->OnMouseDragged(
      AdjustMouseEventLocationForOmniboxView(event));
}

void LocationBarView::OnMouseReleased(const ui::MouseEvent& event) {
  omnibox_view_->OnMouseReleased(AdjustMouseEventLocationForOmniboxView(event));
}

void LocationBarView::OnMouseMoved(const ui::MouseEvent& event) {
  OnOmniboxHovered(true);
}

void LocationBarView::OnMouseExited(const ui::MouseEvent& event) {
  OnOmniboxHovered(false);
}

void LocationBarView::ShowContextMenu(const gfx::Point& p,
                                      ui::MenuSourceType source_type) {
  omnibox_view_->ShowContextMenu(p, source_type);
}

void LocationBarView::WriteDragDataForView(views::View* sender,
                                           const gfx::Point& press_pt,
                                           OSExchangeData* data) {
  DCHECK_NE(GetDragOperationsForView(sender, press_pt),
            ui::DragDropTypes::DRAG_NONE);

  WebContents* web_contents = GetWebContents();
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  gfx::ImageSkia favicon = favicon_driver->GetFavicon().AsImageSkia();
  button_drag_utils::SetURLAndDragImage(web_contents->GetVisibleURL(),
                                        web_contents->GetTitle(), favicon,
                                        nullptr, data);
}

int LocationBarView::GetDragOperationsForView(views::View* sender,
                                              const gfx::Point& p) {
  DCHECK_EQ(location_icon_view_, sender);
  WebContents* web_contents = delegate_->GetWebContents();
  return (web_contents && web_contents->GetVisibleURL().is_valid() &&
          (!GetOmniboxView()->IsEditingOrEmpty()))
             ? (ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK)
             : ui::DragDropTypes::DRAG_NONE;
}

bool LocationBarView::CanStartDragForView(View* sender,
                                          const gfx::Point& press_pt,
                                          const gfx::Point& p) {
  return true;
}

void LocationBarView::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, &hover_animation_);
  RefreshBackground();
}

void LocationBarView::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, &hover_animation_);
  AnimationProgressed(animation);
}

void LocationBarView::AnimationCanceled(const gfx::Animation* animation) {
  DCHECK_EQ(animation, &hover_animation_);
  AnimationProgressed(animation);
}

void LocationBarView::OnChildViewRemoved(View* observed_view, View* child) {
  views::AnimationDelegateViews::OnChildViewRemoved(observed_view, child);
  PreferredSizeChanged();
}

void LocationBarView::OnChanged() {
  // Ensure that background colors get updated on tab-switch.
  RefreshBackground();
  location_icon_view_->Update(/*suppress_animations=*/false,
                              omnibox_view_->model()->PopupIsOpen());
  clear_all_button_->SetVisible(
      omnibox_view_ && omnibox_view_->model()->user_input_in_progress() &&
      !omnibox_view_->GetText().empty() &&
      IsVirtualKeyboardVisible(GetWidget()));
  InvalidateLayout();
  SchedulePaint();
  UpdateChipVisibility();
}

void LocationBarView::OnPopupVisibilityChanged() {
  RefreshBackground();

  // The location icon may change when the popup visibility changes.
  // The page action icons and content setting images may be hidden now.
  // This will also schedule a paint and re-layout.
  UpdateWithoutTabRestore();

  // The focus ring may be hidden or shown when the popup visibility changes.
  if (views::FocusRing::Get(this))
    views::FocusRing::Get(this)->SchedulePaint();

  // We indent the textfield when the popup is open to align to suggestions.
  omnibox_view_->NotifyAccessibilityEvent(ax::mojom::Event::kControlsChanged,
                                          true);
}

const LocationBarModel* LocationBarView::GetLocationBarModel() const {
  return delegate_->GetLocationBarModel();
}

void LocationBarView::OnOmniboxFocused() {
  if (views::FocusRing::Get(this))
    views::FocusRing::Get(this)->SchedulePaint();

  // Only show hover animation in unfocused steady state.  Since focusing
  // the omnibox is intentional, snapping is better than transitioning here.
  hover_animation_.Reset();
  RefreshBackground();
}

void LocationBarView::OnOmniboxBlurred() {
  if (views::FocusRing::Get(this))
    views::FocusRing::Get(this)->SchedulePaint();
  RefreshBackground();
}

void LocationBarView::OnOmniboxHovered(bool is_hovering) {
  if (is_hovering) {
    // Only show the hover animation when omnibox is in unfocused steady state.
    if (!omnibox_view_->HasFocus())
      hover_animation_.Show();
  } else {
    hover_animation_.Hide();
  }
}

void LocationBarView::OnTouchUiChanged() {
  const gfx::FontList& font_list = views::TypographyProvider::Get().GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);
  location_icon_view_->SetFontList(font_list);
  omnibox_view_->SetFontList(font_list);
  if (OmniboxPrefixRichAutocompletionEnabled())
    ime_prefix_autocomplete_view_->SetFontList(font_list);
  ime_inline_autocomplete_view_->SetFontList(font_list);
  if (OmniboxFieldTrial::RichAutocompletionShowAdditionalText())
    omnibox_additional_text_view_->SetFontList(font_list);
  selected_keyword_view_->SetFontList(font_list);
  for (ContentSettingImageView* view : content_setting_views_)
    view->SetFontList(font_list);
  page_action_icon_controller_->SetFontList(font_list);
  location_icon_view_->Update(/*suppress_animations=*/false,
                              omnibox_view_->model()->PopupIsOpen());
  PreferredSizeChanged();
}

bool LocationBarView::ShouldChipOverrideLocationIcon() {
  if (permission_dashboard_view_) {
    return permission_dashboard_view_->GetIndicatorChip()->GetVisible() ||
           permission_dashboard_view_->GetRequestChip()->GetVisible();
  }

  return chip_controller_ && chip_controller_->chip()->GetVisible();
}

bool LocationBarView::IsEditingOrEmpty() const {
  return omnibox_view_ && omnibox_view_->IsEditingOrEmpty();
}

void LocationBarView::OnLocationIconPressed(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton() &&
      ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    std::u16string text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kSelection, /* data_dst = */ nullptr, &text);
    text = OmniboxView::SanitizeTextForPaste(text);

    if (!GetOmniboxView()->model()->CanPasteAndGo(text)) {
      return;
    }

    GetOmniboxView()->model()->PasteAndGo(text, event.time_stamp());
  }
}

void LocationBarView::OnLocationIconDragged(const ui::MouseEvent& event) {
  GetOmniboxView()->CloseOmniboxPopup();
}

SkColor LocationBarView::GetSecurityChipColor(
    security_state::SecurityLevel security_level) const {
  ui::ColorId id = kColorOmniboxText;
  if (security_level == security_state::SECURE_WITH_POLICY_INSTALLED_CERT) {
    id = kColorOmniboxTextDimmed;
  } else if (security_level == security_state::DANGEROUS) {
    id = kColorOmniboxSecurityChipDangerous;
  }

  return GetColorProvider()->GetColor(id);
}

bool LocationBarView::ShowPageInfoDialog() {
  WebContents* contents = GetWebContents();
  if (!contents)
    return false;

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (entry->IsInitialEntry())
    return false;

  DCHECK(GetWidget());

  auto initialized_callback =
      GetPageInfoDialogCreatedCallbackForTesting()
          ? std::move(GetPageInfoDialogCreatedCallbackForTesting())
          : base::DoNothing();

  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          this, gfx::Rect(), GetWidget()->GetNativeWindow(), contents,
          entry->GetVirtualURL(), std::move(initialized_callback),
          base::BindOnce(&LocationBarView::OnPageInfoBubbleClosed,
                         weak_factory_.GetWeakPtr()),
          /*allow_about_this_site=*/true);
  bubble->SetHighlightedButton(location_icon_view_);
  bubble->GetWidget()->Show();
  RecordPageInfoMetrics();
  return true;
}

void LocationBarView::RecordPageInfoMetrics() {
  if (GetChipController()) {
    bool confirmation_chip_collapsed_recently =
        base::TimeTicks::Now() - confirmation_chip_collapsed_time_ <=
        permissions::kConfirmationConsiderationDurationForUma;

    if (!GetChipController()->chip()->GetVisible() &&
        !confirmation_chip_collapsed_recently) {
      permissions::PermissionUmaUtil::RecordPageInfoDialogAccessType(
          permissions::PageInfoDialogAccessType::LOCK_CLICK);
    } else if (GetChipController()->chip()->GetVisible()) {
      permissions::PermissionUmaUtil::RecordPageInfoDialogAccessType(
          permissions::PageInfoDialogAccessType::
              LOCK_CLICK_DURING_CONFIRMATION_CHIP);
    } else {
      permissions::PermissionUmaUtil::RecordPageInfoDialogAccessType(
          permissions::PageInfoDialogAccessType::
              LOCK_CLICK_SHORTLY_AFTER_CONFIRMATION_CHIP);
    }
  } else {
    permissions::PermissionUmaUtil::RecordPageInfoDialogAccessType(
        permissions::PageInfoDialogAccessType::LOCK_CLICK);
  }
}

ui::ImageModel LocationBarView::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) const {
  bool dark_mode = false;
  if (location_icon_view_ && location_icon_view_->GetBackground()) {
    dark_mode =
        color_utils::IsDark(location_icon_view_->GetBackground()->get_color());
  }

  return omnibox_view_
             ? omnibox_view_->GetIcon(
                   GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
                   location_icon_view_->GetForegroundColor(),
                   View::GetColorProvider()->GetColor(kColorOmniboxResultsIcon),
                   View::GetColorProvider()->GetColor(
                       kColorOmniboxResultsStarterPackIcon),
                   View::GetColorProvider()->GetColor(
                       kColorOmniboxAnswerIconGM3Foreground),
                   std::move(on_icon_fetched), dark_mode)
             : ui::ImageModel();
}

void LocationBarView::UpdateChipVisibility() {
  if (!IsEditingOrEmpty()) {
    return;
  }

  if (GetChipController()->chip()->GetVisible()) {
    // If a user starts typing, a permission request should be ignored and the
    // chip finalized.
    GetChipController()->ResetPermissionPromptChip();
  }

  if (base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators)) {
    // Hide the LHS indicator to prevent it appearing in the location bar
    // search panel.
    // This is needed only if the indicator is already visible. If the
    // location bar is in editing mode, we do not show new indicators.
    if (permission_dashboard_view_->GetVisible()) {
      permission_dashboard_view_->SetVisible(false);
    }
  }
}

ui::MouseEvent LocationBarView::AdjustMouseEventLocationForOmniboxView(
    const ui::MouseEvent& event) const {
  ui::MouseEvent adjusted(event);
  adjusted.ConvertLocationToTarget<View>(this, omnibox_view_.get());
  return adjusted;
}

bool LocationBarView::GetPopupMode() const {
  return is_popup_mode_;
}

#if BUILDFLAG(IS_MAC)
void LocationBarView::OnAppShimChanged(const webapps::AppId& app_id) {
  WebContents* web_contents = GetWebContents();
  // During window creation and teardown it is possible for web_contents to be
  // null.
  if (!web_contents) {
    return;
  }
  if (const webapps::AppId* id =
          web_app::WebAppTabHelper::GetAppId(web_contents);
      id && *id == app_id) {
    UpdateContentSettingsIcons();
  }
}
#endif

BEGIN_METADATA(LocationBarView)
ADD_READONLY_PROPERTY_METADATA(int, BorderRadius)
ADD_READONLY_PROPERTY_METADATA(gfx::Point, OmniboxViewOrigin)
ADD_PROPERTY_METADATA(std::u16string, ImePrefixAutocompletion)
ADD_PROPERTY_METADATA(std::u16string, ImeInlineAutocompletion)
ADD_PROPERTY_METADATA(std::u16string, OmniboxAdditionalText)
ADD_READONLY_PROPERTY_METADATA(int, MinimumLeadingWidth)
ADD_READONLY_PROPERTY_METADATA(int, MinimumTrailingWidth)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, LocalBoundsWithoutEndcaps)
ADD_READONLY_PROPERTY_METADATA(bool, PopupMode)
END_METADATA
