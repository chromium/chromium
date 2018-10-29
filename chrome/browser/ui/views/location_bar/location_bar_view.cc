// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/autofill/local_card_migration_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/save_card_icon_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/toolbar_model.h"
#include "components/omnibox/browser/vector_icons.h"
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
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
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
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#endif

namespace {

// This feature shows the full URL when the user focuses the omnibox via
// keyboard shortcut. This feature flag only exists so we have a remote
// killswitch for this behavior.
base::Feature kOmniboxShowFullUrlOnKeyboardShortcut{
    "OmniboxShowFullUrlOnKeyboardShortcut", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

using content::WebContents;
using views::View;

// LocationBarView -----------------------------------------------------------

// static
const char LocationBarView::kViewClassName[] = "LocationBarView";

LocationBarView::LocationBarView(Browser* browser,
                                 Profile* profile,
                                 CommandUpdater* command_updater,
                                 Delegate* delegate,
                                 bool is_popup_mode)
    : LocationBar(profile),
      ChromeOmniboxEditController(command_updater),
      browser_(browser),
      delegate_(delegate),
      is_popup_mode_(is_popup_mode),
      tint_(GetTint()) {
  edit_bookmarks_enabled_.Init(
      bookmarks::prefs::kEditBookmarksEnabled, profile->GetPrefs(),
      base::Bind(&LocationBarView::UpdateWithoutTabRestore,
                 base::Unretained(this)));
  md_observer_.Add(ui::MaterialDesignController::GetInstance());
}

LocationBarView::~LocationBarView() {}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public:

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

  location_icon_view_ = new LocationIconView(font_list, this);
  location_icon_view_->set_drag_controller(this);
  AddChildView(location_icon_view_);

  // Initialize the Omnibox view.
  omnibox_view_ = new OmniboxViewViews(
      this, std::make_unique<ChromeOmniboxClient>(this, profile()),
      is_popup_mode_, this, font_list);
  omnibox_view_->Init();
  AddChildView(omnibox_view_);

  RefreshBackground();

  // Initialize the inline autocomplete view which is visible only when IME is
  // turned on.  Use the same font with the omnibox and highlighted background.
  ime_inline_autocomplete_view_ =
      new views::Label(base::string16(), {font_list});
  ime_inline_autocomplete_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  ime_inline_autocomplete_view_->SetAutoColorReadabilityEnabled(false);
  ime_inline_autocomplete_view_->SetBackground(views::CreateSolidBackground(
      GetColor(OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_BACKGROUND)));
  ime_inline_autocomplete_view_->SetEnabledColor(
      GetColor(OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_TEXT));
  ime_inline_autocomplete_view_->SetVisible(false);
  AddChildView(ime_inline_autocomplete_view_);

  selected_keyword_view_ = new SelectedKeywordView(this, font_list, profile());
  AddChildView(selected_keyword_view_);

  keyword_hint_view_ = new KeywordHintView(this, profile(), tint());
  AddChildView(keyword_hint_view_);

  SkColor icon_color = GetColor(OmniboxPart::RESULTS_ICON);

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    ContentSettingImageView* image_view =
        new ContentSettingImageView(std::move(model), this, font_list);
    image_view->SetIconColor(icon_color);
    content_setting_views_.push_back(image_view);
    image_view->SetVisible(false);
    AddChildView(image_view);
  }

  std::vector<PageActionIconType> page_action_icon_types;
  // |browser_| may be null when LocationBarView is used for non-Browser windows
  // such as PresentationReceiverWindowView, which do not support page actions.
  if (browser_) {
    page_action_icon_types = {PageActionIconType::kFind,
                              PageActionIconType::kZoom};
  }
  page_action_icon_container_view_ = new PageActionIconContainerView(
      page_action_icon_types, GetLayoutConstant(LOCATION_BAR_ICON_SIZE), 0,
      browser_, this, delegate_);
  AddChildView(page_action_icon_container_view_);
  page_action_icon_container_view_->SetIconColor(icon_color);

  manage_passwords_icon_view_ =
      new ManagePasswordsIconViews(command_updater(), this);
  page_action_icons_.push_back(manage_passwords_icon_view_);

  if (browser_) {
    save_credit_card_icon_view_ = new autofill::SaveCardIconView(
        command_updater(), browser_, this, font_list);
    page_action_icons_.push_back(save_credit_card_icon_view_);
  }
  translate_icon_view_ = new TranslateIconView(command_updater(), this);
  page_action_icons_.push_back(translate_icon_view_);
  if (browser_) {
    local_card_migration_icon_view_ = new autofill::LocalCardMigrationIconView(
        command_updater(), browser_, this);
    page_action_icons_.push_back(local_card_migration_icon_view_);
  }

#if defined(OS_CHROMEOS)
  if (browser_)
    page_action_icons_.push_back(intent_picker_view_ =
                                     new IntentPickerView(browser_, this));
#endif
  if (browser_) {
    page_action_icons_.push_back(
        star_view_ = new StarView(command_updater(), browser_, this));
  }

  for (PageActionIconView* icon_view : page_action_icons_) {
    icon_view->Init();
    icon_view->SetVisible(false);
    icon_view->SetIconColor(icon_color);
    AddChildView(icon_view);
  }

  clear_all_button_ = views::CreateVectorImageButton(this);
  clear_all_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_CLEAR_ALL));
  RefreshClearAllButtonIcon();
  AddChildView(clear_all_button_);

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(nullptr);

  size_animation_.Reset(1);

  hover_animation_.SetSlideDuration(200);
}

bool LocationBarView::IsInitialized() const {
  return omnibox_view_ != nullptr;
}

SkColor LocationBarView::GetColor(OmniboxPart part) const {
  return GetOmniboxColor(part, tint());
}

SkColor LocationBarView::GetOpaqueBorderColor(bool incognito) const {
  return color_utils::GetResultingPaintColor(
      GetBorderColor(), ThemeProperties::GetDefaultColor(
                            ThemeProperties::COLOR_TOOLBAR, incognito));
}

int LocationBarView::GetBorderRadius() const {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MAXIMUM, size());
}

SkColor LocationBarView::GetSecurityChipColor(
    security_state::SecurityLevel security_level) const {
  // Only used in ChromeOS.
  if (security_level == security_state::SECURE_WITH_POLICY_INSTALLED_CERT)
    return GetColor(OmniboxPart::LOCATION_BAR_TEXT_DIMMED);

  OmniboxPartState state = OmniboxPartState::CHIP_DEFAULT;
  if (security_level == security_state::EV_SECURE ||
      security_level == security_state::SECURE) {
    state = OmniboxPartState::CHIP_SECURE;
  } else if (security_level == security_state::DANGEROUS) {
    state = OmniboxPartState::CHIP_DANGEROUS;
  }

  return GetOmniboxColor(OmniboxPart::LOCATION_BAR_SECURITY_CHIP, tint(),
                         state);
}

SkColor LocationBarView::GetIconInkDropColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
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

void LocationBarView::SetStarToggled(bool on) {
  if (star_view_)
    star_view_->SetToggled(on);
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

bool LocationBarView::ShowPageInfoDialog(WebContents* contents) {
  DCHECK(contents);
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry)
    return false;

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  DCHECK(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  DCHECK(GetWidget());
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          this, gfx::Rect(), GetWidget()->GetNativeWindow(), profile(),
          contents, entry->GetVirtualURL(), security_info);
  bubble->SetHighlightedButton(location_icon_view());
  bubble->GetWidget()->Show();

  // When the user opens the page info bubble, we also expose the full URL,
  // temporarily disabling Steady State Elisions and Query in Omnibox.
  // We are currently gating this behavior on the Query in Omnibox flag, since
  // it's still under active experimentation.
  if (base::FeatureList::IsEnabled(omnibox::kQueryInOmnibox))
    omnibox_view()->model()->Unelide(true /* exit_query_in_omnibox */);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public LocationBar implementation:

void LocationBarView::FocusLocation(bool select_all) {
  // Only exit Query in Omnibox mode on focus command if the location bar was
  // already focused to begin with, i.e. user presses Ctrl+L twice.
  bool exit_query_in_omnibox = omnibox_view_->HasFocus();

  omnibox_view_->SetFocus();

  if (!select_all)
    return;

  omnibox_view_->SelectAll(true);

  if (base::FeatureList::IsEnabled(kOmniboxShowFullUrlOnKeyboardShortcut))
    omnibox_view()->model()->Unelide(exit_query_in_omnibox);
}

void LocationBarView::Revert() {
  omnibox_view_->RevertAll();
}

OmniboxView* LocationBarView::GetOmniboxView() {
  return omnibox_view_;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public views::View implementation:

bool LocationBarView::HasFocus() const {
  return omnibox_view_->model()->has_focus();
}

void LocationBarView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGroup;
}

gfx::Size LocationBarView::CalculatePreferredSize() const {
  // Compute minimum height.
  gfx::Size min_size(0, GetLayoutConstant(LOCATION_BAR_HEIGHT));

  if (!IsInitialized())
    return min_size;

  min_size.set_height(min_size.height() * size_animation_.GetCurrentValue());

  // Compute width of omnibox-leading content.
  int leading_width = 0;
  if (ShouldShowKeywordBubble()) {
    // The selected keyword view can collapse completely.
  } else if (ShouldShowLocationIconText()) {
    leading_width =
        location_icon_view_->GetMinimumSizeForLabelText(GetLocationIconText())
            .width();
  } else {
    leading_width = GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
                    location_icon_view_->GetMinimumSize().width();
  }

  // Compute width of omnibox-trailing content.
  int trailing_width =
      IncrementalMinimumWidth(translate_icon_view_) +
      IncrementalMinimumWidth(manage_passwords_icon_view_) +
      IncrementalMinimumWidth(page_action_icon_container_view_);
  if (star_view_)
    trailing_width += IncrementalMinimumWidth(star_view_);
  if (save_credit_card_icon_view_)
    trailing_width += IncrementalMinimumWidth(save_credit_card_icon_view_);
  if (local_card_migration_icon_view_)
    trailing_width += IncrementalMinimumWidth(local_card_migration_icon_view_);
#if defined(OS_CHROMEOS)
  if (intent_picker_view_)
    trailing_width += IncrementalMinimumWidth(intent_picker_view_);
#endif  // defined(OS_CHROMEOS)
  for (auto i = content_setting_views_.begin();
       i != content_setting_views_.end(); ++i) {
    trailing_width += IncrementalMinimumWidth((*i));
  }

  min_size.set_width(leading_width + omnibox_view_->GetMinimumSize().width() +
                     2 * GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) -
                     omnibox_view_->GetInsets().width() + trailing_width);
  return min_size;
}

void LocationBarView::Layout() {
  if (!IsInitialized())
    return;

  selected_keyword_view_->SetVisible(false);
  keyword_hint_view_->SetVisible(false);

  const int edge_padding = GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING);
  int leading_edit_item_padding = edge_padding;
  if (OmniboxFieldTrial::IsJogTextfieldOnPopupEnabled()) {
    // With jog enabled, the text should be indented only if these are all true:
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
    leading_edit_item_padding = should_indent ? kTextJogIndentDp : 0;
  }

  // We always subtract the left padding of the OmniboxView itself to allow for
  // an extended I-beam click target without affecting actual layout.
  leading_edit_item_padding -= omnibox_view_->GetInsets().left();

  LocationBarLayout leading_decorations(LocationBarLayout::LEFT_EDGE,
                                        leading_edit_item_padding);
  LocationBarLayout trailing_decorations(LocationBarLayout::RIGHT_EDGE,
                                         edge_padding);

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

  location_icon_view_->SetLabel(base::string16());
  if (ShouldShowKeywordBubble()) {
    location_icon_view_->SetVisible(false);
    leading_decorations.AddDecoration(vertical_padding, location_height, false,
                                      kLeadingDecorationMaxFraction,
                                      edge_padding, selected_keyword_view_);
    if (selected_keyword_view_->keyword() != keyword) {
      selected_keyword_view_->SetKeyword(keyword);
      const TemplateURL* template_url =
          TemplateURLServiceFactory::GetForProfile(profile())->
          GetTemplateURLForKeyword(keyword);
      if (template_url &&
          (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)) {
        gfx::Image image = extensions::OmniboxAPI::Get(profile())->
            GetOmniboxIcon(template_url->GetExtensionId());
        selected_keyword_view_->SetImage(image.AsImageSkia());
      } else {
        selected_keyword_view_->ResetImage();
      }
    }
  } else if (ShouldShowLocationIconText()) {
    location_icon_view_->SetLabel(GetLocationIconText());
    leading_decorations.AddDecoration(vertical_padding, location_height, false,
                                      kLeadingDecorationMaxFraction,
                                      edge_padding, location_icon_view_);
  } else {
    leading_decorations.AddDecoration(vertical_padding, location_height, false,
                                      0, edge_padding, location_icon_view_);
  }

  auto add_trailing_decoration = [&trailing_decorations, vertical_padding,
                                  location_height, edge_padding](View* view) {
    if (view->visible()) {
      trailing_decorations.AddDecoration(vertical_padding, location_height,
                                         false, 0, edge_padding, view);
    }
  };

  if (star_view_)
    add_trailing_decoration(star_view_);
  add_trailing_decoration(page_action_icon_container_view_);
#if defined(OS_CHROMEOS)
  if (intent_picker_view_)
    add_trailing_decoration(intent_picker_view_);
#endif
  add_trailing_decoration(translate_icon_view_);
  if (save_credit_card_icon_view_)
    add_trailing_decoration(save_credit_card_icon_view_);
  if (local_card_migration_icon_view_)
    add_trailing_decoration(local_card_migration_icon_view_);
  add_trailing_decoration(manage_passwords_icon_view_);
  for (ContentSettingViews::const_reverse_iterator i(
           content_setting_views_.rbegin());
       i != content_setting_views_.rend(); ++i) {
    add_trailing_decoration(*i);
  }
  // Because IMEs may eat the tab key, we don't show "press tab to search" while
  // IME composition is in progress.
  if (HasFocus() && !keyword.empty() &&
      omnibox_view_->model()->is_keyword_hint() &&
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

  int location_needed_width = omnibox_view_->GetTextWidth();
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
  if (ime_inline_autocomplete_view_->visible()) {
    int width =
        gfx::GetStringWidth(ime_inline_autocomplete_view_->text(),
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
  omnibox_view_->SetBoundsRect(location_bounds);
  View::Layout();
}

void LocationBarView::OnThemeChanged() {
  tint_ = GetTint();
}

void LocationBarView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  // ToolbarView::Init() adds |this| to the view hierarchy before initializing,
  // which will trigger an early theme change.
  if (!IsInitialized())
    return;

  RefreshBackground();
  RefreshLocationIcon();
  RefreshClearAllButtonIcon();
  SchedulePaint();
}

void LocationBarView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
  SchedulePaint();
}

void LocationBarView::Update(const WebContents* contents) {
  RefreshContentSettingViews();

  RefreshPageActionIconViews();

  if (star_view_) {
    // TODO(calamity): Refactor Update to use PageActionIconView::Refresh.
    UpdateBookmarkStarVisibility();
  }

  if (contents)
    omnibox_view_->OnTabChanged(contents);
  else
    omnibox_view_->Update();

  location_icon_view_->SetTextVisibility(
      ShouldShowLocationIconText(),
      !contents && ShouldAnimateLocationIconTextVisibilityChange());
  OnChanged();  // NOTE: Calls Layout().
  last_update_security_level_ = GetToolbarModel()->GetSecurityLevel(false);
}

void LocationBarView::ResetTabState(WebContents* contents) {
  omnibox_view_->ResetTabState(contents);
}

bool LocationBarView::ActivateFirstInactiveBubbleForAccessibility() {
  if (page_action_icon_container_view_
          ->ActivateFirstInactiveBubbleForAccessibility()) {
    return true;
  }

  auto result = std::find_if(
      page_action_icons_.begin(), page_action_icons_.end(),
      [](PageActionIconView* view) {
        if (!view || !view->visible() || !view->GetBubble())
          return false;

        views::Widget* widget = view->GetBubble()->GetWidget();
        return widget && widget->IsVisible() && !widget->IsActive();
      });

  if (result != page_action_icons_.end())
    (*result)->GetBubble()->GetWidget()->Show();

  return result != page_action_icons_.end();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public OmniboxEditController implementation:

void LocationBarView::UpdateWithoutTabRestore() {
  Update(nullptr);
}

ToolbarModel* LocationBarView::GetToolbarModel() {
  return delegate_->GetToolbarModel();
}

WebContents* LocationBarView::GetWebContents() {
  return delegate_->GetWebContents();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public ContentSettingImageView::Delegate implementation:

SkColor LocationBarView::GetContentSettingInkDropColor() const {
  return GetIconInkDropColor();
}

content::WebContents* LocationBarView::GetContentSettingWebContents() {
  return GetToolbarModel()->input_in_progress() ? nullptr : GetWebContents();
}

ContentSettingBubbleModelDelegate*
LocationBarView::GetContentSettingBubbleModelDelegate() {
  return delegate_->GetContentSettingBubbleModelDelegate();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public PageActionIconView::Delegate implementation:

SkColor LocationBarView::GetPageActionInkDropColor() const {
  return GetIconInkDropColor();
}

WebContents* LocationBarView::GetWebContentsForPageActionIconView() {
  return GetWebContents();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public static methods:

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
      GetLayoutConstant(LOCATION_BAR_BUBBLE_VERTICAL_PADDING) +
      GetLayoutConstant(LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING);
  return std::max(
      0, LocationBarView::GetAvailableTextHeight() - (bubble_padding * 2));
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private:

int LocationBarView::IncrementalMinimumWidth(views::View* view) const {
  return view->visible() ? view->GetMinimumSize().width() : 0;
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
    const SkColor normal = GetOmniboxColor(OmniboxPart::LOCATION_BAR_BACKGROUND,
                                           tint(), OmniboxPartState::NORMAL);
    const SkColor hovered =
        GetOmniboxColor(OmniboxPart::LOCATION_BAR_BACKGROUND, tint(),
                        OmniboxPartState::HOVERED);
    const double opacity = hover_animation_.GetCurrentValue();
    background_color = gfx::Tween::ColorValueBetween(opacity, normal, hovered);
    border_color = GetBorderColor();
  }

  if (is_popup_mode_) {
    SetBackground(views::CreateSolidBackground(background_color));
  } else {
    // High contrast schemes get a border stroke even on a rounded omnibox.
    SkColor stroke_color = GetNativeTheme()->UsesHighContrastColors()
                               ? border_color
                               : SK_ColorTRANSPARENT;
    SetBackground(CreateRoundRectBackground(background_color, stroke_color));
  }

  // Keep the views::Textfield in sync. It needs an opaque background to
  // correctly enable subpixel AA.
  omnibox_view_->SetBackgroundColor(background_color);
  omnibox_view_->EmphasizeURLComponents();

  SchedulePaint();
}

void LocationBarView::RefreshLocationIcon() {
  // Cancel any previous outstanding icon requests, as they are now outdated.
  icon_fetch_weak_ptr_factory_.InvalidateWeakPtrs();

  security_state::SecurityLevel security_level =
      GetToolbarModel()->GetSecurityLevel(false);

  gfx::ImageSkia icon = omnibox_view_->GetIcon(
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetSecurityChipColor(security_level),
      base::BindOnce(&LocationBarView::OnLocationIconFetched,
                     icon_fetch_weak_ptr_factory_.GetWeakPtr()));

  location_icon_view_->SetImage(icon);
  location_icon_view_->Update();
}

void LocationBarView::OnLocationIconFetched(const gfx::Image& image) {
  location_icon_view_->SetImage(image.AsImageSkia());
}

bool LocationBarView::RefreshContentSettingViews() {
  if (extensions::HostedAppBrowserController::IsForExperimentalHostedAppBrowser(
          browser_)) {
    // For hosted apps, the location bar is normally hidden and icons appear in
    // the window frame instead.
    GetWidget()->non_client_view()->ResetWindowControls();
  }

  bool visibility_changed = false;
  for (auto* v : content_setting_views_) {
    const bool was_visible = v->visible();
    v->Update();
    if (was_visible != v->visible())
      visibility_changed = true;
  }
  return visibility_changed;
}

void LocationBarView::RefreshPageActionIconViews() {
  if (extensions::HostedAppBrowserController::IsForExperimentalHostedAppBrowser(
          browser_)) {
    // For hosted apps, the location bar is normally hidden and icons appear in
    // the window frame instead.
    GetWidget()->non_client_view()->ResetWindowControls();
  }

  page_action_icon_container_view_->UpdateAll();

  for (PageActionIconView* icon : page_action_icons_)
    icon->Update();
}

void LocationBarView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  DCHECK(event.IsMouseEvent() || event.IsGestureEvent());
  if (keyword_hint_view_ == sender) {
    omnibox_view_->model()->AcceptKeyword(
        event.IsMouseEvent() ? KeywordModeEntryMethod::CLICK_ON_VIEW
                             : KeywordModeEntryMethod::TAP_ON_VIEW);
  } else {
    DCHECK_EQ(clear_all_button_, sender);
    omnibox_view_->SetUserText(base::string16());
  }
}

void LocationBarView::RefreshClearAllButtonIcon() {
  const gfx::VectorIcon& icon = ui::MaterialDesignController::touch_ui()
                                    ? omnibox::kClearIcon
                                    : kTabCloseNormalIcon;
  SetImageFromVectorIcon(clear_all_button_, icon,
                         GetColor(OmniboxPart::LOCATION_BAR_CLEAR_ALL));
  clear_all_button_->SetBorder(views::CreateEmptyBorder(
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING)));
}

void LocationBarView::RefreshFocusRing() {
  // Popup windows should not display a focus ring.
  if (is_popup_mode_)
    return;

  // We may not have a usable path during initialization before first layout.
  // This will be called again once there is a usable path, so early exit.
  const int radius = GetBorderRadius();
  SkPath path;
  path.addRRect(
      SkRRect::MakeRectXY(RectToSkRect(GetLocalBounds()), radius, radius));
  if (!views::FocusRing::IsPathUseable(path))
    return;

  // Install a focus ring if it's not already there.
  if (!focus_ring_) {
    focus_ring_ = views::FocusRing::Install(this);
    focus_ring_->SetHasFocusPredicate([](View* view) -> bool {
      auto* v = static_cast<LocationBarView*>(view);

      // Show focus ring when the Omnibox is visibly focused and the popup is
      // closed.
      return v->omnibox_view_->model()->is_caret_visible() &&
             !v->GetOmniboxPopupView()->IsOpen();
    });
  }

  // Update the focus ring path.
  focus_ring_->SetPath(path);
}

base::string16 LocationBarView::GetLocationIconText() const {
  if (GetToolbarModel()->GetURL().SchemeIs(content::kChromeUIScheme))
    return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);

  if (GetToolbarModel()->GetURL().SchemeIs(url::kFileScheme))
    return l10n_util::GetStringUTF16(IDS_OMNIBOX_FILE);

  if (delegate_->GetWebContents()) {
    // On ChromeOS, this can be called using web_contents from
    // SimpleWebViewDialog::GetWebContents() which always returns null.
    // TODO(crbug.com/680329) Remove the null check and make
    // SimpleWebViewDialog::GetWebContents return the proper web contents
    // instead.
    const base::string16 extension_name =
        extensions::ui_util::GetEnabledExtensionNameForUrl(
            GetToolbarModel()->GetURL(),
            delegate_->GetWebContents()->GetBrowserContext());
    if (!extension_name.empty())
      return extension_name;
  }

  return GetToolbarModel()->GetSecureVerboseText();
}

bool LocationBarView::ShouldShowKeywordBubble() const {
  return omnibox_view_->model()->is_keyword_selected();
}

bool LocationBarView::ShouldShowLocationIconText() const {
  const auto* toolbar_model = GetToolbarModel();
  if (!toolbar_model->input_in_progress()) {
    const GURL& url = toolbar_model->GetURL();
    if (url.SchemeIs(content::kChromeUIScheme) ||
        url.SchemeIs(extensions::kExtensionScheme) ||
        url.SchemeIs(url::kFileScheme))
      return true;
  }

  return !toolbar_model->GetSecureVerboseText().empty();
}

bool LocationBarView::ShouldAnimateLocationIconTextVisibilityChange() const {
  using SecurityLevel = security_state::SecurityLevel;
  SecurityLevel level = GetToolbarModel()->GetSecurityLevel(false);
  // Do not animate transitions from HTTP_SHOW_WARNING to DANGEROUS, since the
  // transition can look confusing/messy.
  if (level == SecurityLevel::DANGEROUS &&
      last_update_security_level_ == SecurityLevel::HTTP_SHOW_WARNING)
    return false;
  return level == SecurityLevel::DANGEROUS ||
         level == SecurityLevel::HTTP_SHOW_WARNING;
}

OmniboxPopupView* LocationBarView::GetOmniboxPopupView() {
  DCHECK(IsInitialized());
  return omnibox_view_->model()->popup_model()->view();
}

OmniboxTint LocationBarView::GetTint() {
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile());
  if (theme_service->UsingDefaultTheme()) {
    bool is_dark_mode = GetNativeTheme()->SystemDarkModeEnabled();
    return profile()->GetProfileType() == Profile::INCOGNITO_PROFILE ||
                   is_dark_mode
               ? OmniboxTint::DARK
               : OmniboxTint::LIGHT;
  }

  // Check for GTK on Desktop Linux.
  if (theme_service->IsSystemThemeDistinctFromDefaultTheme() &&
      theme_service->UsingSystemTheme())
    return OmniboxTint::NATIVE;

  // TODO(tapted): Infer a tint from theme colors?
  return OmniboxTint::LIGHT;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private LocationBar implementation:

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
  omnibox_view_->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB, false,
                                      match_selection_timestamp);
}

void LocationBarView::FocusSearch() {
  omnibox_view_->SetFocus();
  omnibox_view_->EnterKeywordModeForDefaultSearchProvider();
}

void LocationBarView::UpdateContentSettingsIcons() {
  if (RefreshContentSettingViews()) {
    Layout();
    SchedulePaint();
  }
}

void LocationBarView::UpdateManagePasswordsIconAndBubble() {
  if (manage_passwords_icon_view_->Update()) {
    Layout();
    SchedulePaint();
  }
}

void LocationBarView::UpdateSaveCreditCardIcon() {
  if (save_credit_card_icon_view_->Update()) {
    Layout();
    SchedulePaint();
  }
}

void LocationBarView::UpdateLocalCardMigrationIcon() {
  if (local_card_migration_icon_view_->Update()) {
    Layout();
    SchedulePaint();
  }
}

void LocationBarView::UpdateBookmarkStarVisibility() {
  if (star_view_) {
    star_view_->SetVisible(
        browser_defaults::bookmarks_enabled && !is_popup_mode_ &&
        !GetToolbarModel()->input_in_progress() &&
        edit_bookmarks_enabled_.GetValue() &&
        !IsBookmarkStarHiddenByExtension());
  }
}

void LocationBarView::UpdateLocationBarVisibility(bool visible, bool animate) {
  if (!animate) {
    size_animation_.Reset(visible ? 1 : 0);
    SetVisible(visible);
    return;
  }

  if (visible) {
    SetVisible(true);
    size_animation_.Show();
  } else {
    size_animation_.Hide();
  }
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

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private LocationBarTesting implementation:

bool LocationBarView::GetBookmarkStarVisibility() {
  DCHECK(star_view_);
  return star_view_->visible();
}

bool LocationBarView::TestContentSettingImagePressed(size_t index) {
  if (index >= content_setting_views_.size())
    return false;

  views::View* image_view = content_setting_views_[index];
  image_view->SetSize(gfx::Size(24, 24));
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

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private views::View implementation:

const char* LocationBarView::GetClassName() const {
  return kViewClassName;
}

void LocationBarView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  RefreshBackground();
  RefreshFocusRing();
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
  omnibox_view_->SetFocus();
}

void LocationBarView::OnPaintBorder(gfx::Canvas* canvas) {
  if (!is_popup_mode_)
    return;  // The border is painted by our Background.

  gfx::Rect bounds(GetContentsBounds());
  const SkColor border_color =
      GetOpaqueBorderColor(profile()->IsOffTheRecord());
  BrowserView::Paint1pxHorizontalLine(canvas, border_color, bounds, false);
  BrowserView::Paint1pxHorizontalLine(canvas, border_color, bounds, true);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private views::DragController implementation:

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
          (!GetOmniboxView()->IsEditingOrEmpty())) ?
      (ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK) :
      ui::DragDropTypes::DRAG_NONE;
}

bool LocationBarView::CanStartDragForView(View* sender,
                                          const gfx::Point& press_pt,
                                          const gfx::Point& p) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private gfx::AnimationDelegate implementation:
void LocationBarView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &size_animation_) {
    GetWidget()->non_client_view()->Layout();
  } else if (animation == &hover_animation_) {
    RefreshBackground();
  } else {
    NOTREACHED();
  }
}

void LocationBarView::AnimationEnded(const gfx::Animation* animation) {
  if (animation == &size_animation_) {
    AnimationProgressed(animation);
    if (animation->GetCurrentValue() == 0)
      SetVisible(false);
  } else if (animation == &hover_animation_) {
    AnimationProgressed(animation);
  }
}

void LocationBarView::AnimationCanceled(const gfx::Animation* animation) {
  if (animation == &hover_animation_)
    AnimationProgressed(animation);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private OmniboxEditController implementation:

void LocationBarView::OnChanged() {
  RefreshLocationIcon();
  location_icon_view_->set_show_tooltip(!GetOmniboxView()->IsEditingOrEmpty());
  clear_all_button_->SetVisible(GetToolbarModel()->input_in_progress() &&
                                !omnibox_view_->text().empty() &&
                                IsVirtualKeyboardVisible(GetWidget()));
  Layout();
  SchedulePaint();
}

void LocationBarView::OnPopupVisibilityChanged() {
  RefreshBackground();

  // The focus ring may be hidden or shown when the popup visibility changes.
  if (focus_ring_)
    focus_ring_->SchedulePaint();

  if (OmniboxFieldTrial::IsJogTextfieldOnPopupEnabled()) {
    Layout();
    SchedulePaint();
  }
}

const ToolbarModel* LocationBarView::GetToolbarModel() const {
  return delegate_->GetToolbarModel();
}

void LocationBarView::OnOmniboxFocused() {
  if (focus_ring_)
    focus_ring_->SchedulePaint();

  // Only show hover animation in unfocused steady state.  Since focusing
  // the omnibox is intentional, snapping is better than transitioning here.
  hover_animation_.Reset();

  RefreshBackground();
}

void LocationBarView::OnOmniboxBlurred() {
  if (focus_ring_)
    focus_ring_->SchedulePaint();

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

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private DropdownBarHostDelegate implementation:

void LocationBarView::SetFocusAndSelection(bool select_all) {
  FocusLocation(select_all);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private ui::MaterialDesignControllerObserver implementation:

void LocationBarView::OnTouchUiChanged() {
  const gfx::FontList& font_list = views::style::GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);
  location_icon_view_->SetFontList(font_list);
  omnibox_view_->SetFontList(font_list);
  ime_inline_autocomplete_view_->SetFontList(font_list);
  selected_keyword_view_->SetFontList(font_list);
  for (ContentSettingImageView* view : content_setting_views_)
    view->SetFontList(font_list);
  if (save_credit_card_icon_view_)
    save_credit_card_icon_view_->SetFontList(font_list);
  RefreshLocationIcon();
  Layout();
  SchedulePaint();
}
