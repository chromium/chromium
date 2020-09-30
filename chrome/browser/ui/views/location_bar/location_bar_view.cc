// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
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
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_icon_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/content_settings/core/common/features.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/language_state.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/feature_switch.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {

int IncrementalMinimumWidth(const views::View* view) {
  return (view && view->GetVisible()) ? view->GetMinimumSize().width() : 0;
}

}  // namespace

using content::WebContents;
using metrics::OmniboxEventProto;
using views::View;

// LocationBarView -----------------------------------------------------------

// static
const char LocationBarView::kViewClassName[] = "LocationBarView";

LocationBarView::LocationBarView(Browser* browser,
                                 Profile* profile,
                                 CommandUpdater* command_updater,
                                 Delegate* delegate,
                                 bool is_popup_mode)
    : AnimationDelegateViews(this),
      ChromeOmniboxEditController(command_updater),
      browser_(browser),
      profile_(profile),
      delegate_(delegate),
      is_popup_mode_(is_popup_mode) {
  if (!is_popup_mode_) {
    focus_ring_ = views::FocusRing::Install(this);
    focus_ring_->SetHasFocusPredicate([](View* view) -> bool {
      DCHECK_EQ(view->GetClassName(), LocationBarView::kViewClassName);
      auto* v = static_cast<LocationBarView*>(view);

      // Show focus ring when the Omnibox is visibly focused and the popup is
      // closed.
      return v->omnibox_view_->model()->is_caret_visible() &&
             !v->GetOmniboxPopupView()->IsOpen();
    });

    focus_ring_->SetPathGenerator(
        std::make_unique<views::PillHighlightPathGenerator>());
  }
}

LocationBarView::~LocationBarView() {}

void LocationBarView::Init() {
  // We need to be in a Widget, otherwise GetNativeTheme() may change and we're
  // not prepared for that.
  DCHECK(GetWidget());

  // Note that children with layers are *not* clipped, because focus rings have
  // to draw outside the parent's bounds.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  const gfx::FontList& font_list = views::style::GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);

  auto location_icon_view =
      std::make_unique<LocationIconView>(font_list, this, this);
  location_icon_view->set_drag_controller(this);
  location_icon_view_ = AddChildView(std::move(location_icon_view));

  // Initialize the Omnibox view.
  auto omnibox_view = std::make_unique<OmniboxViewViews>(
      this, std::make_unique<ChromeOmniboxClient>(this, profile_),
      is_popup_mode_, this, font_list);
  omnibox_view->Init();
  omnibox_view_ = AddChildView(std::move(omnibox_view));

  // Initiate the Omnibox additional-text label.
  if (OmniboxFieldTrial::RichAutocompletionShowAdditionalText()) {
    // TODO (manukh) When the titles UI is disabled,
    // |omnibox_additional_text_view| will only contain URLs and never page
    // titles. It can safely be styled with STYLE_LINK. When the titles UI is
    // enabled, it can contain either URLs or page titles. Ideally, its style
    // would be updated appropriately, but given early consensus suggests titles
    // UI is unlikely to launch, we don't have to worry about this case for now.
    auto style = OmniboxFieldTrial::RichAutocompletionShowTitles()
                     ? views::style::STYLE_PRIMARY
                     : views::style::STYLE_LINK;
    auto omnibox_additional_text_view = std::make_unique<views::Label>(
        base::string16(), ChromeTextContext::CONTEXT_OMNIBOX_DEEMPHASIZED,
        style);
    omnibox_additional_text_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    omnibox_additional_text_view_ =
        AddChildView(std::move(omnibox_additional_text_view));
  }

  RefreshBackground();

  // Initialize the inline autocomplete view which is visible only when IME is
  // turned on.  Use the same font with the omnibox and highlighted background.
  auto ime_inline_autocomplete_view = std::make_unique<views::Label>(
      base::string16(), views::Label::CustomFont{font_list});
  ime_inline_autocomplete_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  ime_inline_autocomplete_view->SetAutoColorReadabilityEnabled(false);
  ime_inline_autocomplete_view->SetBackground(views::CreateSolidBackground(
      GetOmniboxColor(GetThemeProvider(), OmniboxPart::LOCATION_BAR_BACKGROUND,
                      OmniboxPartState::SELECTED)));
  ime_inline_autocomplete_view->SetEnabledColor(GetOmniboxColor(
      GetThemeProvider(), OmniboxPart::LOCATION_BAR_TEXT_DEFAULT,
      OmniboxPartState::SELECTED));
  ime_inline_autocomplete_view->SetVisible(false);
  ime_inline_autocomplete_view_ =
      AddChildView(std::move(ime_inline_autocomplete_view));

  selected_keyword_view_ =
      AddChildView(std::make_unique<SelectedKeywordView>(this, font_list));

  keyword_hint_view_ = AddChildView(std::make_unique<KeywordHintView>(
      base::BindRepeating(&LocationBarView::KeywordHintViewPressed,
                          base::Unretained(this)),
      profile_));

  SkColor icon_color = GetColor(OmniboxPart::RESULTS_ICON);

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), this, this, font_list);
    image_view->SetIconColor(icon_color);
    image_view->SetVisible(false);
    content_setting_views_.push_back(AddChildView(std::move(image_view)));
  }

  PageActionIconParams params;
  // |browser_| may be null when LocationBarView is used for non-Browser windows
  // such as PresentationReceiverWindowView, which do not support page actions.
  if (browser_) {
    // The send tab to self icon is intentionally the first one added so it is
    // the left most icon.
    params.types_enabled.push_back(PageActionIconType::kSendTabToSelf);
    if (base::FeatureList::IsEnabled(kClickToCallUI))
      params.types_enabled.push_back(PageActionIconType::kClickToCall);
    if (base::FeatureList::IsEnabled(kSharingQRCodeGenerator))
      params.types_enabled.push_back(PageActionIconType::kQRCodeGenerator);
    if (base::FeatureList::IsEnabled(kSharedClipboardUI))
      params.types_enabled.push_back(PageActionIconType::kSharedClipboard);
    if (!base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableToolbarStatusChip)) {
      params.types_enabled.push_back(PageActionIconType::kManagePasswords);
    }
    params.types_enabled.push_back(PageActionIconType::kIntentPicker);
    params.types_enabled.push_back(PageActionIconType::kPwaInstall);
    params.types_enabled.push_back(PageActionIconType::kFind);
    params.types_enabled.push_back(PageActionIconType::kTranslate);
    params.types_enabled.push_back(PageActionIconType::kZoom);
    params.types_enabled.push_back(PageActionIconType::kNativeFileSystemAccess);

    if (dom_distiller::IsDomDistillerEnabled() && browser_->is_type_normal()) {
      params.types_enabled.push_back(PageActionIconType::kReaderMode);
    }
    params.types_enabled.push_back(PageActionIconType::kCookieControls);
  }
  // Add icons only when feature is not enabled. Otherwise icons will
  // be added to the ToolbarPageActionIconContainerView.
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableToolbarStatusChip)) {
    params.types_enabled.push_back(PageActionIconType::kSaveCard);
    params.types_enabled.push_back(PageActionIconType::kLocalCardMigration);
  }
  if (browser_ && !is_popup_mode_)
    params.types_enabled.push_back(PageActionIconType::kBookmarkStar);

  params.icon_color = icon_color;
  params.between_icon_spacing = 0;
  params.font_list = &font_list;
  params.browser = browser_;
  params.command_updater = command_updater();
  params.icon_label_bubble_delegate = this;
  params.page_action_icon_delegate = this;
  page_action_icon_container_ =
      AddChildView(std::make_unique<PageActionIconContainerView>(params));
  page_action_icon_controller_ = page_action_icon_container_->controller();

  auto clear_all_button = views::CreateVectorImageButton(base::BindRepeating(
      static_cast<void (OmniboxView::*)(const base::string16&)>(
          &OmniboxView::SetUserText),
      base::Unretained(omnibox_view_), base::string16()));
  clear_all_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_CLEAR_ALL));
  clear_all_button_ = AddChildView(std::move(clear_all_button));
  RefreshClearAllButtonIcon();

  permission_chip_ = AddChildView(std::make_unique<PermissionChip>(browser()));

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(nullptr);

  hover_animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(200));

  is_initialized_ = true;
}

bool LocationBarView::IsInitialized() const {
  return is_initialized_;
}

SkColor LocationBarView::GetColor(OmniboxPart part) const {
  DCHECK(GetWidget());
  return GetOmniboxColor(GetThemeProvider(), part);
}

SkColor LocationBarView::GetOpaqueBorderColor() const {
  return color_utils::GetResultingPaintColor(
      GetBorderColor(),
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR));
}

int LocationBarView::GetBorderRadius() const {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MAXIMUM, size());
}

std::unique_ptr<views::Background> LocationBarView::CreateRoundRectBackground(
    SkColor background_color,
    SkColor stroke_color,
    SkBlendMode blend_mode,
    bool antialias) const {
  const int radius = GetBorderRadius();
  auto painter =
      stroke_color == SK_ColorTRANSPARENT
          ? views::Painter::CreateSolidRoundRectPainter(
                background_color, radius, gfx::Insets(), blend_mode, antialias)
          : views::Painter::CreateRoundRectWith1PxBorderPainter(
                background_color, stroke_color, radius, blend_mode, antialias);
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

void LocationBarView::SetImeInlineAutocompletion(const base::string16& text) {
  ime_inline_autocomplete_view_->SetText(text);
  ime_inline_autocomplete_view_->SetVisible(!text.empty());
}

void LocationBarView::SelectAll() {
  omnibox_view_->SelectAll(true);
}

void LocationBarView::FocusLocation(bool is_user_initiated) {
  const bool omnibox_already_focused = omnibox_view_->HasFocus();

  if (is_user_initiated)
    omnibox_view()->model()->Unelide();

  omnibox_view_->SetFocus(is_user_initiated);

  if (omnibox_already_focused)
    omnibox_view()->model()->ClearKeyword();

  if (is_user_initiated)
    omnibox_view_->SelectAll(true);
}

void LocationBarView::Revert() {
  omnibox_view_->RevertAll();
}

OmniboxView* LocationBarView::GetOmniboxView() {
  return omnibox_view_;
}

bool LocationBarView::HasFocus() const {
  return omnibox_view_ && omnibox_view_->model()->has_focus();
}

void LocationBarView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGroup;
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

gfx::Size LocationBarView::CalculatePreferredSize() const {
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

void LocationBarView::Layout() {
  if (!IsInitialized())
    return;

  selected_keyword_view_->SetVisible(false);
  keyword_hint_view_->SetVisible(false);

  const int edge_padding = GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING);

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

  // We have an odd indent value because this is what matches the odd text
  // indent value in OmniboxMatchCellView.
  constexpr int kTextJogIndentDp = 11;
  int leading_edit_item_padding = should_indent ? kTextJogIndentDp : 0;

  // We always subtract the left padding of the OmniboxView itself to allow for
  // an extended I-beam click target without affecting actual layout.
  leading_edit_item_padding -= omnibox_view_->GetInsets().left();

  LocationBarLayout leading_decorations(LocationBarLayout::Position::kLeftEdge,
                                        leading_edit_item_padding);
  LocationBarLayout trailing_decorations(
      LocationBarLayout::Position::kRightEdge, edge_padding);

  const base::string16 keyword(omnibox_view_->model()->keyword());
  // In some cases (e.g. fullscreen mode) we may have 0 height.  We still want
  // to position our child views in this case, because other things may be
  // positioned relative to them (e.g. the "bookmark added" bubble if the user
  // hits ctrl-d).
  const int vertical_padding = GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING);
  const int location_height = std::max(height() - (vertical_padding * 2), 0);
  // The largest fraction of the omnibox that can be taken by the EV or search
  // label/chip.
  const double kLeadingDecorationMaxFraction = 0.5;

  if (permission_chip_->GetVisible() && !ShouldShowKeywordBubble()) {
    leading_decorations.AddDecoration(vertical_padding, location_height, false,
                                      0, edge_padding, permission_chip_);
  }

  if (ShouldShowKeywordBubble()) {
    location_icon_view_->SetVisible(false);
    leading_decorations.AddDecoration(vertical_padding, location_height, false,
                                      kLeadingDecorationMaxFraction,
                                      edge_padding, selected_keyword_view_);
    if (selected_keyword_view_->keyword() != keyword) {
      selected_keyword_view_->SetKeyword(keyword, profile_);
      const TemplateURL* template_url =
          TemplateURLServiceFactory::GetForProfile(profile_)
              ->GetTemplateURLForKeyword(keyword);
      gfx::Image image;
      if (template_url &&
          (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)) {
        image = extensions::OmniboxAPI::Get(profile_)->GetOmniboxIcon(
            template_url->GetExtensionId());
      } else if (template_url && template_url->type() == TemplateURL::NORMAL &&
                 OmniboxFieldTrial::IsExperimentalKeywordModeEnabled()) {
        image =
            omnibox_view()
                ->model()
                ->client()
                ->GetFaviconForKeywordSearchProvider(
                    template_url,
                    base::BindOnce(&LocationBarView::OnKeywordFaviconFetched,
                                   base::Unretained(this)));
      }
      selected_keyword_view_->SetCustomImage(image);
    }
  } else if (location_icon_view_->ShouldShowText()) {
    leading_decorations.AddDecoration(vertical_padding, location_height, false,
                                      kLeadingDecorationMaxFraction,
                                      edge_padding, location_icon_view_);
  } else {
    leading_decorations.AddDecoration(vertical_padding, location_height, false,
                                      0, edge_padding, location_icon_view_);
  }

  auto add_trailing_decoration = [&trailing_decorations, vertical_padding,
                                  location_height, edge_padding](View* view) {
    if (view->GetVisible()) {
      trailing_decorations.AddDecoration(vertical_padding, location_height,
                                         false, 0, edge_padding, view);
    }
  };

  add_trailing_decoration(page_action_icon_container_);
  for (ContentSettingViews::const_reverse_iterator i(
           content_setting_views_.rbegin());
       i != content_setting_views_.rend(); ++i) {
    add_trailing_decoration(*i);
  }
  // Because IMEs may eat the tab key, we don't show "press tab to search" while
  // IME composition is in progress.
  // The keyword hint is also not shown when the keyword button is enabled since
  // it's redundant with that and is no longer accurate.
  if (!OmniboxFieldTrial::IsKeywordSearchButtonEnabled() && HasFocus() &&
      !keyword.empty() && omnibox_view_->model()->is_keyword_hint() &&
      !omnibox_view_->IsImeComposing()) {
    trailing_decorations.AddDecoration(vertical_padding, location_height, true,
                                       0, edge_padding, keyword_hint_view_);
    keyword_hint_view_->SetKeyword(keyword);
  }

  add_trailing_decoration(clear_all_button_);

  // Perform layout.
  int entry_width = width();
  leading_decorations.LayoutPass1(&entry_width);
  trailing_decorations.LayoutPass1(&entry_width);
  leading_decorations.LayoutPass2(&entry_width);
  trailing_decorations.LayoutPass2(&entry_width);

  // Compute widths needed for location bar.
  int location_needed_width = omnibox_view_->GetTextWidth();
  if (OmniboxFieldTrial::RichAutocompletionShowAdditionalText()) {
    // Calculate location_needed_width based on the omnibox view and omnibox
    // additional text widths. If RichAutocompletionTwoLineOmnibox is enabled,
    // location_needed_width only needs to be large enough to contain the
    // larger; otherwise, it must be large enough to contain both in addition to
    // the padding in between.
    int omnibox_additional_text_needed_width =
        omnibox_additional_text_view_->CalculatePreferredSize().width();
    location_needed_width =
        OmniboxFieldTrial::RichAutocompletionTwoLineOmnibox()
            ? std::max(location_needed_width,
                       omnibox_additional_text_needed_width)
            : location_needed_width + omnibox_additional_text_needed_width + 10;
    // TODO (manukh): If we launch rich autocompletion with the current
    //  iteration of 1 line UI, the padding (10) should  be moved to
    //  layout_constants.cc. Likewise below.
  }

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
  entry_width = location_bounds.width();

  // Layout |ime_inline_autocomplete_view_| next to the user input.
  if (ime_inline_autocomplete_view_->GetVisible()) {
    int width =
        gfx::GetStringWidth(ime_inline_autocomplete_view_->GetText(),
                            ime_inline_autocomplete_view_->font_list()) +
        ime_inline_autocomplete_view_->GetInsets().width();
    // All the target languages (IMEs) are LTR, and we do not need to support
    // RTL so far.  In other words, no testable RTL environment so far.
    int x = location_needed_width;
    if (width > entry_width)
      x = 0;
    else if (location_needed_width + width > entry_width)
      x = entry_width - width;
    location_bounds.set_width(x);
    ime_inline_autocomplete_view_->SetBounds(
        location_bounds.right(), location_bounds.y(),
        std::min(width, entry_width), location_bounds.height());
  }

  // If rich autocompletion is enabled, split |location_bounds| for the
  // |omnibox_view_| and |omnibox_additional_text_view_|.
  if (OmniboxFieldTrial::RichAutocompletionShowAdditionalText() &&
      OmniboxFieldTrial::RichAutocompletionTwoLineOmnibox()) {
    // Split vertically.
    auto omnibox_bounds = location_bounds;
    omnibox_bounds.set_height(location_bounds.height() / 2);
    omnibox_view_->SetBoundsRect(omnibox_bounds);
    auto omnibox_additional_text_bounds = omnibox_bounds;
    omnibox_additional_text_bounds.set_x(location_bounds.x() + 3);
    omnibox_additional_text_bounds.set_y(omnibox_bounds.bottom());
    omnibox_additional_text_view_->SetBoundsRect(
        omnibox_additional_text_bounds);

  } else if (OmniboxFieldTrial::RichAutocompletionShowAdditionalText() &&
             !omnibox_view_->GetText().empty()) {
    // Split horizontally.
    auto omnibox_bounds = location_bounds;
    omnibox_bounds.set_width(std::min(
        omnibox_view_->GetUnelidedTextWidth() + 10, location_bounds.width()));
    omnibox_view_->SetBoundsRect(omnibox_bounds);
    auto omnibox_additional_text_bounds = location_bounds;
    omnibox_additional_text_bounds.set_x(omnibox_bounds.x() +
                                         omnibox_bounds.width());
    omnibox_additional_text_bounds.set_width(
        std::max(location_bounds.width() - omnibox_bounds.width(), 0));
    omnibox_additional_text_view_->SetBoundsRect(
        omnibox_additional_text_bounds);
  } else {
    omnibox_view_->SetBoundsRect(location_bounds);
  }

  View::Layout();
}

void LocationBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // ToolbarView::Init() adds |this| to the view hierarchy before initializing,
  // which will trigger an early theme change.
  if (!IsInitialized())
    return;

  SkColor icon_color = GetColor(OmniboxPart::RESULTS_ICON);
  page_action_icon_controller_->SetIconColor(icon_color);
  for (ContentSettingImageView* image_view : content_setting_views_)
    image_view->SetIconColor(icon_color);

  RefreshBackground();
  RefreshClearAllButtonIcon();
}

void LocationBarView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
  SchedulePaint();
}

void LocationBarView::SetOmniboxAdditionalText(const base::string16& text) {
  DCHECK(OmniboxFieldTrial::IsRichAutocompletionEnabled() || text.empty());
  if (!OmniboxFieldTrial::RichAutocompletionShowAdditionalText())
    return;
  auto wrappedText =
      text.empty() ? text
                   : base::UTF8ToUTF16("(") + text + base::UTF8ToUTF16(")");
  omnibox_additional_text_view_->SetText(wrappedText);
}

void LocationBarView::Update(WebContents* contents) {
  RefreshContentSettingViews();

  RefreshPageActionIconViews();
  location_icon_view_->Update(/*suppress_animations=*/contents);

  if (contents)
    omnibox_view_->OnTabChanged(contents);
  else
    omnibox_view_->Update();

  PageActionIconView* send_tab_to_self_icon =
      page_action_icon_controller_->GetIconView(
          PageActionIconType::kSendTabToSelf);
  if (send_tab_to_self_icon)
    send_tab_to_self_icon->SetVisible(false);

  PageActionIconView* qr_generator_icon =
      page_action_icon_controller_->GetIconView(
          PageActionIconType::kQRCodeGenerator);
  if (qr_generator_icon)
    qr_generator_icon->SetVisible(false);

  OnChanged();  // NOTE: Calls Layout().
}

void LocationBarView::ResetTabState(WebContents* contents) {
  omnibox_view_->ResetTabState(contents);
}

bool LocationBarView::ActivateFirstInactiveBubbleForAccessibility() {
  return page_action_icon_controller_
      ->ActivateFirstInactiveBubbleForAccessibility();
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
  return GetColor(OmniboxPart::LOCATION_BAR_TEXT_DEFAULT);
}

SkColor LocationBarView::GetIconLabelBubbleBackgroundColor() const {
  return GetColor(OmniboxPart::LOCATION_BAR_BACKGROUND);
}

bool LocationBarView::ShouldHideContentSettingImage() {
  // Content setting icons are hidden at the same time as page action icons.
  return ShouldHidePageActionIcons();
}

content::WebContents* LocationBarView::GetContentSettingWebContents() {
  return GetWebContents();
}

ContentSettingBubbleModelDelegate*
LocationBarView::GetContentSettingBubbleModelDelegate() {
  return delegate_->GetContentSettingBubbleModelDelegate();
}

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
  return omnibox_view_->model()->popup_model()->IsOpen();
}

// static
bool LocationBarView::IsVirtualKeyboardVisible(views::Widget* widget) {
  if (auto* input_method = widget->GetInputMethod()) {
    auto* keyboard = input_method->GetInputMethodKeyboardController();
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

  if (location_icon_view_->ShouldShowText())
    return location_icon_view_->GetMinimumLabelTextWidth();

  return GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
         location_icon_view_->GetMinimumSize().width();
}

int LocationBarView::GetMinimumTrailingWidth() const {
  int trailing_width = IncrementalMinimumWidth(page_action_icon_container_);

  for (auto* content_setting_view : content_setting_views_)
    trailing_width += IncrementalMinimumWidth(content_setting_view);

  return trailing_width;
}

SkColor LocationBarView::GetBorderColor() const {
  return GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_LOCATION_BAR_BORDER);
}

gfx::Rect LocationBarView::GetLocalBoundsWithoutEndcaps() const {
  const int border_radius = height() / 2;
  gfx::Rect bounds_without_endcaps(GetLocalBounds());
  bounds_without_endcaps.Inset(border_radius, 0);
  return bounds_without_endcaps;
}

void LocationBarView::RefreshBackground() {
  // Match the background color to the popup if the Omnibox is visibly focused.
  SkColor background_color, border_color;
  if (omnibox_view_->model()->is_caret_visible()) {
    background_color = border_color = GetColor(OmniboxPart::RESULTS_BACKGROUND);
  } else {
    const SkColor normal = GetColor(OmniboxPart::LOCATION_BAR_BACKGROUND);
    const SkColor hovered = GetOmniboxColor(
        GetThemeProvider(), OmniboxPart::LOCATION_BAR_BACKGROUND,
        OmniboxPartState::HOVERED);
    const double opacity = hover_animation_.GetCurrentValue();
    background_color = gfx::Tween::ColorValueBetween(opacity, normal, hovered);
    border_color = GetBorderColor();
  }

  if (is_popup_mode_) {
    SetBackground(views::CreateSolidBackground(background_color));
  } else {
    SkColor stroke_color = SK_ColorTRANSPARENT;

    if (GetNativeTheme()->UsesHighContrastColors()) {
      // High contrast schemes get a border stroke even on a rounded omnibox.
      stroke_color = border_color;
    }

    SetBackground(CreateRoundRectBackground(background_color, stroke_color));
  }

  // Keep the views::Textfield in sync. It needs an opaque background to
  // correctly enable subpixel AA.
  omnibox_view_->SetBackgroundColor(background_color);

  SchedulePaint();
}

bool LocationBarView::RefreshContentSettingViews() {
  if (web_app::AppBrowserController::IsForWebAppBrowser(browser_)) {
    // For hosted apps, the location bar is normally hidden and icons appear in
    // the window frame instead.
    GetWidget()->non_client_view()->ResetWindowControls();
  }

  bool visibility_changed = false;
  for (auto* v : content_setting_views_) {
    const bool was_visible = v->GetVisible();
    v->Update();
    if (was_visible != v->GetVisible())
      visibility_changed = true;
  }
  return visibility_changed;
}

void LocationBarView::RefreshPageActionIconViews() {
  if (web_app::AppBrowserController::IsForWebAppBrowser(browser_)) {
    // For hosted apps, the location bar is normally hidden and icons appear in
    // the window frame instead.
    GetWidget()->non_client_view()->ResetWindowControls();
  }

  page_action_icon_controller_->UpdateAll();
}

void LocationBarView::RefreshClearAllButtonIcon() {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  const gfx::VectorIcon& icon =
      touch_ui ? omnibox::kClearIcon : kTabCloseNormalIcon;
  SetImageFromVectorIcon(clear_all_button_, icon,
                         GetColor(OmniboxPart::LOCATION_BAR_CLEAR_ALL));
  clear_all_button_->SetBorder(views::CreateEmptyBorder(
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING)));
}

bool LocationBarView::ShouldShowKeywordBubble() const {
  return omnibox_view_->model()->is_keyword_selected();
}

OmniboxPopupView* LocationBarView::GetOmniboxPopupView() {
  DCHECK(IsInitialized());
  return omnibox_view_->model()->popup_model()->view();
}

void LocationBarView::KeywordHintViewPressed(const ui::Event& event) {
  DCHECK(event.IsMouseEvent() || event.IsGestureEvent());
  omnibox_view_->model()->AcceptKeyword(event.IsMouseEvent()
                                            ? OmniboxEventProto::CLICK_HINT_VIEW
                                            : OmniboxEventProto::TAP_HINT_VIEW);
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

GURL LocationBarView::GetDestinationURL() const {
  return destination_url();
}

WindowOpenDisposition LocationBarView::GetWindowOpenDisposition() const {
  return disposition();
}

ui::PageTransition LocationBarView::GetPageTransition() const {
  return transition();
}

base::TimeTicks LocationBarView::GetMatchSelectionTimestamp() const {
  return match_selection_timestamp();
}

void LocationBarView::AcceptInput() {
  AcceptInput(base::TimeTicks());
}

void LocationBarView::AcceptInput(base::TimeTicks match_selection_timestamp) {
  omnibox_view_->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB,
                                      match_selection_timestamp);
}

void LocationBarView::FocusSearch() {
  // This is called by keyboard accelerator, so it's user-initiated.
  omnibox_view_->SetFocus(/*is_user_initiated=*/true);
  omnibox_view_->EnterKeywordModeForDefaultSearchProvider();
}

void LocationBarView::UpdateContentSettingsIcons() {
  if (RefreshContentSettingViews()) {
    Layout();
    SchedulePaint();
  }
}

inline void LocationBarView::UpdateQRCodeGeneratorIcon() {
  PageActionIconView* icon = page_action_icon_controller_->GetIconView(
      PageActionIconType::kQRCodeGenerator);
  if (icon)
    icon->Update();
}

inline bool LocationBarView::UpdateSendTabToSelfIcon() {
  PageActionIconView* icon = page_action_icon_controller_->GetIconView(
      PageActionIconType::kSendTabToSelf);
  if (!icon)
    return false;
  bool was_visible = icon->GetVisible();
  icon->Update();
  return was_visible != icon->GetVisible();
}

void LocationBarView::SaveStateToContents(WebContents* contents) {
  omnibox_view_->SaveStateToTab(contents);
}

const OmniboxView* LocationBarView::GetOmniboxView() const {
  return omnibox_view_;
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
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE));
  image_view->OnKeyReleased(
      ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SPACE, ui::EF_NONE));
  return true;
}

bool LocationBarView::IsContentSettingBubbleShowing(size_t index) {
  return index < content_setting_views_.size() &&
         content_setting_views_[index]->IsBubbleShowing();
}

const char* LocationBarView::GetClassName() const {
  return kViewClassName;
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
  // Renderer-initated focuses go through the FocusLocation() call instead.
  omnibox_view_->SetFocus(/*is_user_initiated=*/true);
}

void LocationBarView::OnPaintBorder(gfx::Canvas* canvas) {
  if (!is_popup_mode_)
    return;  // The border is painted by our Background.

  gfx::Rect bounds(GetContentsBounds());
  const SkColor border_color = GetOpaqueBorderColor();
  canvas->DrawLine(gfx::PointF(bounds.x(), bounds.y()),
                   gfx::PointF(bounds.right(), bounds.y()), border_color);
  canvas->DrawLine(gfx::PointF(bounds.x(), bounds.bottom() - 1),
                   gfx::PointF(bounds.right(), bounds.bottom() - 1),
                   border_color);
}

bool LocationBarView::OnMousePressed(const ui::MouseEvent& event) {
  return omnibox_view_->OnMousePressed(event);
}

bool LocationBarView::OnMouseDragged(const ui::MouseEvent& event) {
  return omnibox_view_->OnMouseDragged(event);
}

void LocationBarView::OnMouseReleased(const ui::MouseEvent& event) {
  omnibox_view_->OnMouseReleased(event);
}

void LocationBarView::OnMouseMoved(const ui::MouseEvent& event) {
  OnOmniboxHovered(true);
}

void LocationBarView::OnMouseExited(const ui::MouseEvent& event) {
  OnOmniboxHovered(false);
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
  button_drag_utils::SetURLAndDragImage(web_contents->GetURL(),
                                        web_contents->GetTitle(), favicon,
                                        nullptr, *sender->GetWidget(), data);
}

int LocationBarView::GetDragOperationsForView(views::View* sender,
                                              const gfx::Point& p) {
  DCHECK_EQ(location_icon_view_, sender);
  WebContents* web_contents = delegate_->GetWebContents();
  return (web_contents && web_contents->GetURL().is_valid() &&
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

void LocationBarView::OnChanged() {
  location_icon_view_->Update(/*suppress_animations=*/false);
  clear_all_button_->SetVisible(
      omnibox_view_ && omnibox_view_->model()->user_input_in_progress() &&
      !omnibox_view_->GetText().empty() &&
      IsVirtualKeyboardVisible(GetWidget()));
  Layout();
  SchedulePaint();
  UpdateSendTabToSelfIcon();
  UpdateQRCodeGeneratorIcon();
}

void LocationBarView::OnPopupVisibilityChanged() {
  RefreshBackground();

  // The location icon may change when the popup visibility changes.
  // The page action icons and content setting images may be hidden now.
  // This will also schedule a paint and re-layout.
  UpdateWithoutTabRestore();

  // The focus ring may be hidden or shown when the popup visibility changes.
  if (focus_ring_)
    focus_ring_->SchedulePaint();

  // We indent the textfield when the popup is open to align to suggestions.
  omnibox_view_->NotifyAccessibilityEvent(ax::mojom::Event::kControlsChanged,
                                          true);
}

const LocationBarModel* LocationBarView::GetLocationBarModel() const {
  return delegate_->GetLocationBarModel();
}

void LocationBarView::OnOmniboxFocused() {
  if (focus_ring_)
    focus_ring_->SchedulePaint();

  // Only show hover animation in unfocused steady state.  Since focusing
  // the omnibox is intentional, snapping is better than transitioning here.
  hover_animation_.Reset();

  UpdateSendTabToSelfIcon();
  UpdateQRCodeGeneratorIcon();
  RefreshBackground();
}

void LocationBarView::OnOmniboxBlurred() {
  if (focus_ring_)
    focus_ring_->SchedulePaint();
  UpdateSendTabToSelfIcon();
  UpdateQRCodeGeneratorIcon();
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

void LocationBarView::FocusAndSelectAll() {
  FocusLocation(true);
}

void LocationBarView::OnTouchUiChanged() {
  const gfx::FontList& font_list = views::style::GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);
  location_icon_view_->SetFontList(font_list);
  omnibox_view_->SetFontList(font_list);
  ime_inline_autocomplete_view_->SetFontList(font_list);
  selected_keyword_view_->SetFontList(font_list);
  for (ContentSettingImageView* view : content_setting_views_)
    view->SetFontList(font_list);
  page_action_icon_controller_->SetFontList(font_list);
  location_icon_view_->Update(/*suppress_animations=*/false);
  PreferredSizeChanged();
}

bool LocationBarView::IsEditingOrEmpty() const {
  return omnibox_view_ && omnibox_view_->IsEditingOrEmpty();
}

void LocationBarView::OnLocationIconPressed(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton() &&
      ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    base::string16 text;
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
  return GetOmniboxSecurityChipColor(GetThemeProvider(), security_level);
}

bool LocationBarView::ShowPageInfoDialog() {
  WebContents* contents = GetWebContents();
  if (!contents)
    return false;

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry)
    return false;

  DCHECK(GetWidget());
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          this, gfx::Rect(), GetWidget()->GetNativeWindow(), profile_, contents,
          entry->GetVirtualURL(),
          base::BindOnce(&LocationBarView::OnPageInfoBubbleClosed,
                         weak_factory_.GetWeakPtr()));
  bubble->SetHighlightedButton(location_icon_view_);
  bubble->GetWidget()->Show();
  return true;
}

ui::ImageModel LocationBarView::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) const {
  return omnibox_view_
             ? omnibox_view_->GetIcon(GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
                                      location_icon_view_->GetForegroundColor(),
                                      std::move(on_icon_fetched))
             : ui::ImageModel();
}
