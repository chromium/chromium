// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/string_compare.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/md_text_button_with_down_arrow.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

bool UseGoogleTranslateBranding() {
  // Only use Google Translate branding in Chrome branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

std::unique_ptr<views::View> CreateWordmarkView() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::BoxLayout>());

  // Translate icon
  const int translate_icon_id = IDR_TRANSLATE_TAB_WORDMARK;
  std::unique_ptr<views::ImageView> translate_icon =
      std::make_unique<views::ImageView>();
  gfx::ImageSkia* translate_icon_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          translate_icon_id);
  translate_icon->SetImage(*translate_icon_image);
  view->AddChildView(std::move(translate_icon));

  return view;
#else
  return nullptr;
#endif
}

void OpenLanguageSettings(TranslateBubbleModel* model_,
                          content::WebContents* web_contents_) {
  model_->ReportUIInteraction(translate::UIInteraction::kOpenLanguageSettings);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!profile) {
    return;
  }

  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  Browser* browser = displayer.browser();
  if (!browser) {
    return;
  }

  chrome::ShowSettingsSubPage(browser, chrome::kLanguageOptionsSubPage);
  return;
}

}  // namespace

TranslateBubbleView::~TranslateBubbleView() {
  // A child view could refer to a model which is owned by this class when
  // the child view is destructed. For example, |source_language_combobx_model_|
  // is referred by Combobox's destructor. Before destroying the models,
  // removing the child views is needed.
  RemoveAllChildViews();
  if (features::IsToolbarPinningEnabled() && translate_action_item_) {
    translate_action_item_->SetIsShowingBubble(false);
  }
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView, kIdentifier);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView, kSourceLanguageTab);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView, kTargetLanguageTab);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView, kCloseButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView, kOptionsMenuButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView,
                                      kChangeTargetLanguage);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView,
                                      kTargetLanguageCombobox);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView,
                                      kTargetLanguageDoneButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView,
                                      kChangeSourceLanguage);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView,
                                      kSourceLanguageCombobox);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView,
                                      kSourceLanguageDoneButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView, kErrorMessage);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TranslateBubbleView,
                                      kOpenLanguageSettings);

void TranslateBubbleView::CloseBubble() {
  mouse_handler_.reset();
  LocationBarBubbleDelegateView::CloseBubble();
}

void TranslateBubbleView::TabSelectedAt(int index) {
  // Tabbed pane is indexed from left to right starting at 0.
  if (!model_->IsPageTranslatedInCurrentLanguages() && index == 1) {
    Translate();
  } else if (index == 0) {
    ShowOriginal();
  }
}

void TranslateBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  should_always_translate_ = model_->ShouldAlwaysTranslate();
  should_never_translate_language_ = model_->ShouldNeverTranslateLanguage();
  should_never_translate_site_ = model_->ShouldNeverTranslateSite();
  translate_view_ = AddChildView(CreateView());
  advanced_view_source_ = AddChildView(CreateViewAdvancedSource());
  advanced_view_target_ = AddChildView(CreateViewAdvancedTarget());
  error_view_ = AddChildView(CreateViewError());

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));

  UpdateChildVisibilities();

  if (GetViewState() == TranslateBubbleModel::VIEW_STATE_ERROR) {
    model_->ShowError(error_type_);
  }

  if (features::IsToolbarPinningEnabled()) {
    Browser* browser = chrome::FindLastActive();
    if (browser) {
      translate_action_item_ = actions::ActionManager::Get().FindAction(
          kActionShowTranslate, browser->browser_actions()->root_action_item());
      CHECK(translate_action_item_);
      translate_action_item_->SetIsShowingBubble(true);
    }
  }
}

views::View* TranslateBubbleView::GetInitiallyFocusedView() {
  return GetCurrentView()->GetNextFocusableView();
}

bool TranslateBubbleView::ShouldShowCloseButton() const {
  return false;
}

bool TranslateBubbleView::ShouldShowWindowTitle() const {
  return false;
}

bool TranslateBubbleView::DidLanguageSelectionChange(
    TranslateBubbleModel::ViewState view_state) {
  if (view_state == TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE) {
    return source_language_combobox_->GetSelectedIndex() !=
           previous_source_language_index_;
  } else {
    return target_language_combobox_->GetSelectedIndex() !=
           previous_target_language_index_;
  }
}

void TranslateBubbleView::ResetLanguage() {
  if (GetViewState() == TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE) {
    source_language_combobox_->SetSelectedIndex(
        previous_source_language_index_);
    model_->UpdateSourceLanguageIndex(
        source_language_combobox_->GetSelectedIndex().value());
  } else {
    target_language_combobox_->SetSelectedIndex(
        previous_target_language_index_);
    model_->UpdateTargetLanguageIndex(
        target_language_combobox_->GetSelectedIndex().value());
  }
  UpdateAdvancedView();
}

void TranslateBubbleView::WindowClosing() {
  // The operations for |model_| are valid only when a WebContents is alive.
  // TODO(crbug.com/40341719): TranslateBubbleViewModel(Impl) should not hold a
  // WebContents as a member variable because the WebContents might be destroyed
  // while the TranslateBubbleViewModel(Impl) is still alive. Instead,
  // TranslateBubbleViewModel should take a reference of a WebContents at each
  // method.
  if (web_contents()) {
    model_->OnBubbleClosing();
  }

  // We have to reset the controller reference to the view here, not in our
  // destructor, because we'll be destroyed asynchronously and the shown state
  // will be checked before then.
  if (on_closing_) {
    std::move(on_closing_).Run();
  }
}

bool TranslateBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  switch (GetViewState()) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        Translate();
        return true;
      }
      break;
    }
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        ShowOriginal();
        return true;
      }
      break;
    }
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      break;
    case TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      break;
    case TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE:
      break;
  }
  return BubbleDialogDelegateView::AcceleratorPressed(accelerator);
}

gfx::Size TranslateBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int width = 0;
  for (const views::View* child : children()) {
    width = std::max(width, child->GetPreferredSize().width());
  }
  return gfx::Size(width, GetCurrentView()->GetPreferredSize().height());
}

// Create the menu items for the dropdown options menu under TAB UI.
void TranslateBubbleView::ShowOptionsMenu(views::Button* source) {
  // Recreate the menu model as translated languages can change while the menu
  // is not showing, which invalidates these text strings.
  options_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  options_menu_model_->AddItemWithStringId(
      OptionsMenuItem::CHANGE_TARGET_LANGUAGE,
      IDS_TRANSLATE_BUBBLE_CHANGE_TARGET_LANGUAGE);
  options_menu_model_->SetElementIdentifierAt(
      options_menu_model_->GetItemCount() - 1, kChangeTargetLanguage);

  auto source_language_code = model_->GetSourceLanguageCode();
  auto source_language =
      model_->GetSourceLanguageNameAt(model_->GetSourceLanguageIndex());

  // Don't show "Always translate <language>" in incognito mode, because it
  // doesn't do anything anyways. Don't show if the source language is unknown.
  if (!is_in_incognito_window_ &&
      source_language_code != translate::kUnknownLanguageCode) {
    options_menu_model_->AddCheckItem(
        OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE,
        l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
                                   source_language));
  }

  if (source_language_code != translate::kUnknownLanguageCode) {
    options_menu_model_->AddCheckItem(
        OptionsMenuItem::NEVER_TRANSLATE_LANGUAGE,
        l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
                                   source_language));
  }

  if (model_->CanAddSiteToNeverPromptList()) {
    options_menu_model_->AddCheckItem(
        OptionsMenuItem::NEVER_TRANSLATE_SITE,
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE));
  }

  options_menu_model_->AddItem(
      OptionsMenuItem::CHANGE_SOURCE_LANGUAGE,
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_CHANGE_SOURCE_LANGUAGE,
                                 source_language));
  options_menu_model_->SetElementIdentifierAt(
      options_menu_model_->GetItemCount() - 1, kChangeSourceLanguage);

  if (!is_in_incognito_window_ &&
      base::FeatureList::IsEnabled(language::kTranslateOpenSettings)) {
    options_menu_model_->AddItem(
        OptionsMenuItem::OPEN_LANGUAGE_SETTINGS,
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPEN_LANGUAGE_SETTINGS));
    options_menu_model_->SetElementIdentifierAt(
        options_menu_model_->GetItemCount() - 1, kOpenLanguageSettings);
  }

  options_menu_runner_ = std::make_unique<views::MenuRunner>(
      options_menu_model_.get(), views::MenuRunner::COMBOBOX);
  gfx::Rect screen_bounds = source->GetAnchorBoundsInScreen();
  options_menu_runner_->RunMenuAt(source->GetWidget(), nullptr, screen_bounds,
                                  views::MenuAnchorPosition::kTopRight,
                                  ui::MENU_SOURCE_MOUSE);
}

bool TranslateBubbleView::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case OptionsMenuItem::NEVER_TRANSLATE_LANGUAGE:
      return should_never_translate_language_;
    case OptionsMenuItem::NEVER_TRANSLATE_SITE:
      return should_never_translate_site_;
    case OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE:
      return should_always_translate_;
    default:
      NOTREACHED();
  }
}

bool TranslateBubbleView::IsCommandIdEnabled(int command_id) const {
  return true;
}

void TranslateBubbleView::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE:
      should_always_translate_ = !should_always_translate_;
      model_->SetAlwaysTranslate(should_always_translate_);
      if (should_always_translate_) {
        should_never_translate_language_ = false;
        model_->SetNeverTranslateLanguage(should_never_translate_language_);
        if (GetViewState() ==
            TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) {
          model_->Translate();
          SwitchView(TranslateBubbleModel::VIEW_STATE_TRANSLATING);
        }
      }
      UpdateChildVisibilities();
      break;

    case OptionsMenuItem::NEVER_TRANSLATE_LANGUAGE:
      should_never_translate_language_ = !should_never_translate_language_;
      if (should_never_translate_language_) {
        should_always_translate_ = false;
        model_->SetAlwaysTranslate(should_always_translate_);
        model_->SetNeverTranslateLanguage(true);
        RevertOrDeclineTranslation();
      } else {
        model_->SetNeverTranslateLanguage(false);
      }
      break;

    case OptionsMenuItem::NEVER_TRANSLATE_SITE:
      should_never_translate_site_ = !should_never_translate_site_;
      if (should_never_translate_site_) {
        model_->SetNeverTranslateSite(true);
        RevertOrDeclineTranslation();
      } else {
        model_->SetNeverTranslateSite(false);
      }
      break;

    case OptionsMenuItem::CHANGE_TARGET_LANGUAGE:
      SwitchView(TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);
      break;

    case OptionsMenuItem::CHANGE_SOURCE_LANGUAGE:
      SwitchView(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);
      break;

    case OptionsMenuItem::OPEN_LANGUAGE_SETTINGS:
      OpenLanguageSettings(model(), web_contents());
      break;

    default:
      NOTREACHED();
  }
}

void TranslateBubbleView::OnWidgetClosing(views::Widget* widget) {
  if (GetBubbleFrameView()->GetWidget()->closed_reason() ==
      views::Widget::ClosedReason::kCloseButtonClicked) {
    model_->DeclineTranslation();
    model_->ReportUIInteraction(translate::UIInteraction::kCloseUIExplicitly);
  } else {
    model_->ReportUIInteraction(translate::UIInteraction::kCloseUILostFocus);
  }
}

TranslateBubbleModel::ViewState TranslateBubbleView::GetViewState() const {
  return model_->GetViewState();
}

void TranslateBubbleView::SetViewState(translate::TranslateStep step,
                                       translate::TranslateErrors error_type) {
  if (step == translate::TRANSLATE_STEP_TRANSLATE_ERROR) {
    SwitchToErrorView(error_type);
  } else {
    TranslateBubbleModel::ViewState state =
        TranslateBubbleModelImpl::TranslateStepToViewState(step);
    SwitchView(state);
  }
}

TranslateBubbleView::TranslateBubbleView(
    views::View* anchor_view,
    std::unique_ptr<TranslateBubbleModel> model,
    translate::TranslateErrors error_type,
    content::WebContents* web_contents,
    base::OnceClosure on_closing)
    : LocationBarBubbleDelegateView(anchor_view,
                                    web_contents,
                                    /*autosize=*/true),
      model_(std::move(model)),
      error_type_(error_type),
      is_in_incognito_window_(
          web_contents && web_contents->GetBrowserContext()->IsOffTheRecord()),
      on_closing_(std::move(on_closing)) {
  UpdateInsets(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);

  if (web_contents) {  // web_contents can be null in unit_tests.
    mouse_handler_ =
        std::make_unique<WebContentMouseHandler>(this, web_contents);
  }
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetFootnoteView(CreateWordmarkView());
  SetProperty(views::kElementIdentifierKey, kIdentifier);
}

views::View* TranslateBubbleView::GetCurrentView() const {
  switch (GetViewState()) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE:
      return translate_view_;
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
      return translate_view_;
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE:
      return translate_view_;
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      return error_view_;
    case TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE:
      return advanced_view_source_;
    case TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      return advanced_view_target_;
  }
  NOTREACHED();
}

void TranslateBubbleView::Translate() {
  model_->ReportUIInteraction(translate::UIInteraction::kTranslate);
  model_->Translate();
  SwitchView(TranslateBubbleModel::VIEW_STATE_TRANSLATING);
}

void TranslateBubbleView::ShowOriginal() {
  model_->ReportUIInteraction(translate::UIInteraction::kRevert);
  model_->RevertTranslation();
  SwitchView(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
}

void TranslateBubbleView::ConfirmAdvancedOptions() {
  model_->SetAlwaysTranslate(should_always_translate_);
  if (model_->IsPageTranslatedInCurrentLanguages()) {
    SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  } else {
    std::u16string source_language_name;
    std::u16string target_language_name;
    DCHECK(tabbed_pane_);
    UpdateLanguageNames(&source_language_name, &target_language_name);
    tabbed_pane_->GetTabAt(0)->SetTitleText(source_language_name);
    tabbed_pane_->GetTabAt(1)->SetTitleText(target_language_name);
    model_->Translate();
    tabbed_pane_->SelectTabAt(1);
    SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  }
}

void TranslateBubbleView::SourceLanguageChanged() {
  model_->ReportUIInteraction(translate::UIInteraction::kChangeSourceLanguage);
  model_->UpdateSourceLanguageIndex(
      source_language_combobox_->GetSelectedIndex().value());
  UpdateAdvancedView();
}

void TranslateBubbleView::TargetLanguageChanged() {
  model_->ReportUIInteraction(translate::UIInteraction::kChangeTargetLanguage);
  model_->UpdateTargetLanguageIndex(
      target_language_combobox_->GetSelectedIndex().value());
  UpdateAdvancedView();
}

void TranslateBubbleView::AlwaysTranslatePressed() {
  should_always_translate_ = GetAlwaysTranslateCheckbox()->GetChecked();
  // In the tab UI the always translate button should apply immediately
  // except for in an advanced view.
  if (GetViewState() != TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE) {
    model_->SetAlwaysTranslate(should_always_translate_);
    if (!model_->IsPageTranslatedInCurrentLanguages() &&
        should_always_translate_) {
      Translate();
    }
  }
}

void TranslateBubbleView::UpdateChildVisibilities() {
  // Update the state of the always translate checkbox
  if (advanced_always_translate_checkbox_) {
    advanced_always_translate_checkbox_->SetChecked(should_always_translate_);
  }
  if (always_translate_checkbox_) {
    always_translate_checkbox_->SetText(l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
        model_->GetSourceLanguageNameAt(model_->GetSourceLanguageIndex())));
    always_translate_checkbox_->SetChecked(should_always_translate_);
  }
  for (views::View* view : children()) {
    view->SetVisible(view == GetCurrentView());
  }
}

std::unique_ptr<views::View> TranslateBubbleView::CreateEmptyPane() {
  auto pane = std::make_unique<views::View>();
  pane->SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_DIALOG_SUBSECTION)));
  return pane;
}

std::unique_ptr<views::View> TranslateBubbleView::CreateView() {
  std::u16string source_language_name;
  std::u16string target_language_name;
  UpdateLanguageNames(&source_language_name, &target_language_name);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  auto inner_view = std::make_unique<views::View>();
  inner_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
  auto* horizontal_view = view->AddChildView(std::move(inner_view));

  views::View* icon = nullptr;
  if (!UseGoogleTranslateBranding()) {
    icon = horizontal_view->AddChildView(CreateTranslateIcon());
  }

  // Tabbed pane for language selection. Can't use unique_ptr because
  // tabs have to be added after the tabbed_pane is added to the parent,
  // when we release ownership of the unique_ptr.
  auto tabbed_pane = std::make_unique<views::TabbedPane>();
  tabbed_pane_ = horizontal_view->AddChildView(std::move(tabbed_pane));

  // NOTE: Panes must be added after |tabbed_pane| has been added to its
  // parent.
  tabbed_pane_->AddTab(source_language_name, CreateEmptyPane());
  tabbed_pane_->AddTab(target_language_name, CreateEmptyPane());
  tabbed_pane_->GetTabAt(0)->SetProperty(views::kElementIdentifierKey,
                                         kSourceLanguageTab);
  tabbed_pane_->GetTabAt(1)->SetProperty(views::kElementIdentifierKey,
                                         kTargetLanguageTab);
  tabbed_pane_->GetTabAt(0)->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(2, 20)));
  tabbed_pane_->GetTabAt(1)->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(2, 20)));
  tabbed_pane_->set_listener(this);

  auto* padding_view =
      horizontal_view->AddChildView(std::make_unique<views::View>());
  auto* options_menu = horizontal_view->AddChildView(CreateOptionsMenuButton());
  horizontal_view->AddChildView(CreateCloseButton());

  // Don't show the the always translate checkbox if the source language is
  // unknown.
  auto source_language_code = model_->GetSourceLanguageCode();
  if (model_->ShouldShowAlwaysTranslateShortcut() &&
      source_language_code != translate::kUnknownLanguageCode) {
    auto before_always_translate_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
            model_->GetSourceLanguageNameAt(model_->GetSourceLanguageIndex())),
        base::BindRepeating(&TranslateBubbleView::AlwaysTranslatePressed,
                            base::Unretained(this)));
    before_always_translate_checkbox->SetID(BUTTON_ID_ALWAYS_TRANSLATE);
    always_translate_checkbox_ =
        view->AddChildView(std::move(before_always_translate_checkbox));
  }

  const int button_horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  if (icon) {
    icon->SetProperty(views::kMarginsKey,
                      gfx::Insets().set_right(button_horizontal_spacing));
  }
  tabbed_pane_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  padding_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2));
  options_menu->SetProperty(views::kElementIdentifierKey, kOptionsMenuButton);
  options_menu->SetProperty(views::kMarginsKey,
                            gfx::Insets::VH(0, button_horizontal_spacing));
  if (always_translate_checkbox_) {
    horizontal_view->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0,
                          provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_VERTICAL),
                          0));
    always_translate_checkbox_->SetProperty(views::kMarginsKey,
                                            gfx::Insets::VH(2, 0));
  }

  return view;
}

// TODO(crbug.com/40336858): Revise this later to show a specific message for
// each error.
std::unique_ptr<views::View> TranslateBubbleView::CreateViewError() {
  auto translate_options_button =
      std::make_unique<views::MdTextButtonWithDownArrow>(
          views::Button::PressedCallback(),
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  translate_options_button->SetCallback(base::BindRepeating(
      &TranslateBubbleView::ShowOptionsMenu, base::Unretained(this),
      base::Unretained(translate_options_button.get())));
  std::u16string translate_options_button_label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  translate_options_button->GetViewAccessibility().SetName(
      translate_options_button_label);
  translate_options_button->SetTooltipText(translate_options_button_label);
  translate_options_button->SetID(BUTTON_ID_OPTIONS_MENU);
  translate_options_button->SetRequestFocusOnPress(true);
  return CreateViewErrorNoTitle(std::move(translate_options_button));
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewErrorNoTitle(
    std::unique_ptr<views::Button> advanced_button) {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto view = std::make_unique<views::View>();
  views::BoxLayout* layout =
      view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_between_child_spacing(
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  // Title row.
  auto title_row = std::make_unique<views::View>();
  title_row->SetLayoutManager(std::make_unique<views::FlexLayout>());
  int error_message_id = IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE;
  auto error_message_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(error_message_id),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  error_message_label->SetLineHeight(vertical_spacing * 5);
  error_message_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  error_message_label->SetProperty(views::kElementIdentifierKey, kErrorMessage);
  error_message_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  title_row->AddChildView(std::move(error_message_label));
  title_row->AddChildView(CreateCloseButton());
  view->AddChildView(std::move(title_row));

  // Button row.
  auto button_row = std::make_unique<views::BoxLayoutView>();
  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  button_row->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  auto try_again_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](TranslateBubbleModel* model) { model->Translate(); },
          base::Unretained(model_.get())),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRY_AGAIN));
  try_again_button->SetID(BUTTON_ID_TRY_AGAIN);
  button_row->AddChildView(std::move(try_again_button));
  button_row->AddChildView(std::move(advanced_button));
  view->AddChildView(std::move(button_row));

  return view;
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewAdvancedSource() {
  // Bubble title
  std::unique_ptr<views::Label> source_language_title_label =
      std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_SOURCE),
          views::style::CONTEXT_DIALOG_TITLE);

  // Language icon
  int source_default_index = model_->GetSourceLanguageIndex();
  auto source_language_combobox_model =
      std::make_unique<SourceLanguageComboboxModel>(source_default_index,
                                                    model_.get());
  auto source_language_combobox = std::make_unique<views::Combobox>(
      std::move(source_language_combobox_model));
  source_language_combobox->SetProperty(views::kElementIdentifierKey,
                                        kSourceLanguageCombobox);

  // In an incognito window or when the source language is unknown, "Always
  // translate" checkbox shouldn't be shown.
  std::unique_ptr<views::Checkbox> advanced_always_translate_checkbox;
  auto source_language_code = model_->GetSourceLanguageCode();
  if (!is_in_incognito_window_ &&
      source_language_code != translate::kUnknownLanguageCode) {
    advanced_always_translate_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS),
        base::BindRepeating(&TranslateBubbleView::AlwaysTranslatePressed,
                            base::Unretained(this)));
    advanced_always_translate_checkbox->SetID(BUTTON_ID_ALWAYS_TRANSLATE);
  }

  source_language_combobox->SetCallback(base::BindRepeating(
      &TranslateBubbleView::SourceLanguageChanged, base::Unretained(this)));
  source_language_combobox->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_TRANSLATE_BUBBLE_SOURCE_LANG_COMBOBOX_ACCNAME));
  source_language_combobox_ = source_language_combobox.get();

  auto advanced_reset_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&TranslateBubbleView::ResetLanguage,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_RESET));
  advanced_reset_button->SetID(BUTTON_ID_RESET);
  advanced_reset_button_source_ = advanced_reset_button.get();

  auto advanced_done_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&TranslateBubbleView::ConfirmAdvancedOptions,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_DONE));
  advanced_done_button->SetID(BUTTON_ID_DONE);
  advanced_done_button->SetIsDefault(true);
  advanced_done_button_source_ = advanced_done_button.get();
  advanced_done_button_source_->SetProperty(views::kElementIdentifierKey,
                                            kSourceLanguageDoneButton);

  return CreateViewAdvanced(std::move(source_language_combobox),
                            std::move(source_language_title_label),
                            std::move(advanced_reset_button),
                            std::move(advanced_done_button),
                            std::move(advanced_always_translate_checkbox));
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewAdvancedTarget() {
  // Bubble title
  std::unique_ptr<views::Label> target_language_title_label =
      std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_TARGET),
          views::style::CONTEXT_DIALOG_TITLE);

  int target_default_index = model_->GetTargetLanguageIndex();
  auto target_language_combobox_model =
      std::make_unique<TargetLanguageComboboxModel>(target_default_index,
                                                    model_.get());
  auto target_language_combobox = std::make_unique<views::Combobox>(
      std::move(target_language_combobox_model));
  target_language_combobox->SetProperty(views::kElementIdentifierKey,
                                        kTargetLanguageCombobox);

  target_language_combobox->SetCallback(base::BindRepeating(
      &TranslateBubbleView::TargetLanguageChanged, base::Unretained(this)));
  target_language_combobox->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_TRANSLATE_BUBBLE_TARGET_LANG_COMBOBOX_ACCNAME));
  target_language_combobox_ = target_language_combobox.get();

  auto advanced_reset_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&TranslateBubbleView::ResetLanguage,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_RESET));
  advanced_reset_button->SetID(BUTTON_ID_RESET);
  advanced_reset_button_target_ = advanced_reset_button.get();

  auto advanced_done_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&TranslateBubbleView::ConfirmAdvancedOptions,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_DONE));
  advanced_done_button->SetID(BUTTON_ID_DONE);
  advanced_done_button->SetIsDefault(true);
  advanced_done_button_target_ = advanced_done_button.get();
  advanced_done_button_target_->SetProperty(views::kElementIdentifierKey,
                                            kTargetLanguageDoneButton);

  return CreateViewAdvanced(std::move(target_language_combobox),
                            std::move(target_language_title_label),
                            std::move(advanced_reset_button),
                            std::move(advanced_done_button), nullptr);
}

std::unique_ptr<views::View> TranslateBubbleView::CreateViewAdvanced(
    std::unique_ptr<views::Combobox> combobox,
    std::unique_ptr<views::Label> language_title_label,
    std::unique_ptr<views::Button> advanced_reset_button,
    std::unique_ptr<views::Button> advanced_done_button,
    std::unique_ptr<views::Checkbox> advanced_always_translate_checkbox) {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL);

  auto view = std::make_unique<views::BoxLayoutView>();
  view->SetBetweenChildSpacing(horizontal_spacing);

  std::unique_ptr<views::ImageView> language_icon = CreateTranslateIcon();
  if (!UseGoogleTranslateBranding()) {
    // If the bottom branding isn't showing, display the leading Translate
    // icon otherwise it's not obvious what the bubble is about. This should
    // only happen on non-Chrome-branded builds.
    auto* icon_view =
        view->AddChildView(std::make_unique<views::BoxLayoutView>());
    icon_view->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    icon_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
    icon_view->AddChildView(std::move(language_icon));
    icon_view->SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(vertical_spacing, 0));
  }
  auto* form_view = view->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(vertical_spacing)
          .Build());
  // Stretch |form_view| to fit the rest of bubble's width. Note that because no
  // other view has flex set, the flex argument here can be any positive
  // integer.
  view->SetFlexForView(form_view, 1);

  language_title_label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(vertical_spacing, 0, vertical_spacing,
                        horizontal_spacing * 4));
  language_title_label->SetProperty(views::kCrossAxisAlignmentKey,
                                    views::LayoutAlignment::kStart);
  auto* title_row = form_view->AddChildView(std::make_unique<views::View>());
  title_row->SetLayoutManager(std::make_unique<views::FlexLayout>());
  auto* title_label = title_row->AddChildView(std::move(language_title_label));
  auto* padding_view = title_row->AddChildView(std::make_unique<views::View>());
  title_row->AddChildView(CreateCloseButton());
  // Set flex specifications for |title_row| views.
  title_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  // |padding_view| is unbounded so that the close button stays right aligned
  // when the bubble expands.
  padding_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2));

  form_view->AddChildView(std::move(combobox));

  auto button_row = std::make_unique<views::BoxLayoutView>();
  if (advanced_always_translate_checkbox) {
    advanced_always_translate_checkbox_ =
        form_view->AddChildView(std::move(advanced_always_translate_checkbox));
    button_row->SetProperty(views::kMarginsKey,
                            gfx::Insets().set_top(vertical_spacing));
  } else {
    button_row->SetProperty(views::kMarginsKey,
                            gfx::Insets().set_top(2 * vertical_spacing));
  }

  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  button_row->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  button_row->AddChildView(std::move(advanced_reset_button));
  button_row->AddChildView(std::move(advanced_done_button));
  form_view->AddChildView(std::move(button_row));

  UpdateAdvancedView();

  return view;
}

std::unique_ptr<views::ImageView> TranslateBubbleView::CreateTranslateIcon() {
  const int language_icon_id = IDR_TRANSLATE_BUBBLE_ICON;
  std::unique_ptr<views::ImageView> language_icon =
      std::make_unique<views::ImageView>();
  language_icon->SetImage(ui::ImageModel::FromResourceId(language_icon_id));
  return language_icon;
}

std::unique_ptr<views::Button> TranslateBubbleView::CreateOptionsMenuButton() {
  // Three dots options menu button
  auto tab_translate_options_button =
      views::CreateVectorImageButtonWithNativeTheme(
          views::Button::PressedCallback(), kBrowserToolsIcon);
  tab_translate_options_button->SetCallback(base::BindRepeating(
      &TranslateBubbleView::ShowOptionsMenu, base::Unretained(this),
      base::Unretained(tab_translate_options_button.get())));
  InstallCircleHighlightPathGenerator(tab_translate_options_button.get());
  std::u16string translate_options_button_label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  tab_translate_options_button->GetViewAccessibility().SetName(
      translate_options_button_label);
  tab_translate_options_button->SetTooltipText(translate_options_button_label);
  tab_translate_options_button->SetRequestFocusOnPress(true);
  tab_translate_options_button->SetVisible(true);
  tab_translate_options_button->SetID(BUTTON_ID_OPTIONS_MENU);
  return tab_translate_options_button;
}

std::unique_ptr<views::Button> TranslateBubbleView::CreateCloseButton() {
  auto close_button =
      views::BubbleFrameView::CreateCloseButton(base::BindRepeating(
          [](View* view) {
            view->GetWidget()->CloseWithReason(
                views::Widget::ClosedReason::kCloseButtonClicked);
          },
          base::Unretained(this)));
  close_button->SetProperty(views::kElementIdentifierKey, kCloseButton);
  close_button->SetVisible(true);
  close_button->SetID(BUTTON_ID_CLOSE);
  return close_button;
}

views::Checkbox* TranslateBubbleView::GetAlwaysTranslateCheckbox() {
  const TranslateBubbleModel::ViewState state = GetViewState();
  if (state == TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
      state == TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
    return advanced_always_translate_checkbox_;
  }
  CHECK(state == TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE ||
        state == TranslateBubbleModel::VIEW_STATE_TRANSLATING ||
        state == TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  return always_translate_checkbox_;
}

void TranslateBubbleView::SetWindowTitle(
    TranslateBubbleModel::ViewState view_state) {
  switch (view_state) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE:
      SetTitle(IDS_TRANSLATE_BUBBLE_BEFORE_TRANSLATE_TITLE);
      break;
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
      SetTitle(IDS_TRANSLATE_BUBBLE_TRANSLATING);
      break;
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE:
      SetTitle(IDS_TRANSLATE_BUBBLE_TRANSLATED_TITLE);
      break;
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      SetTitle(IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE);
      break;
    case TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE:
      SetTitle(IDS_TRANSLATE_BUBBLE_ADVANCED_SOURCE);
      break;
    case TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      SetTitle(IDS_TRANSLATE_BUBBLE_ADVANCED_TARGET);
      break;
  }
}

void TranslateBubbleView::UpdateViewState(
    TranslateBubbleModel::ViewState view_state) {
  model_->SetViewState(view_state);
  SetWindowTitle(view_state);
}

void TranslateBubbleView::SwitchView(
    TranslateBubbleModel::ViewState view_state) {
  UpdateInsets(view_state);

  if (view_state == TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
      view_state == TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
    GetBubbleFrameView()->SetFootnoteView(nullptr);
  } else {
    GetBubbleFrameView()->SetFootnoteView(CreateWordmarkView());
  }

  SwitchTabForViewState(view_state);

  UpdateViewState(view_state);
  if (view_state == TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
      view_state == TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
    UpdateAdvancedView();
  }

  UpdateChildVisibilities();

  if (view_state == TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE) {
    AnnounceTextToScreenReader(l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_BUBBLE_TRANSLATION_COMPLETE_ANNOUNCEMENT,
        model_->GetTargetLanguageNameAt(model_->GetTargetLanguageIndex())));
  } else if (view_state == TranslateBubbleModel::VIEW_STATE_ERROR) {
    AnnounceTextToScreenReader(l10n_util::GetStringUTF16(
        IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE));
  }
}

void TranslateBubbleView::AnnounceTextToScreenReader(
    const std::u16string& announcement_text) {
  GetViewAccessibility().AnnounceText(announcement_text);
}

void TranslateBubbleView::SwitchTabForViewState(
    TranslateBubbleModel::ViewState view_state) {
  if ((view_state == TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE ||
       view_state == TranslateBubbleModel::VIEW_STATE_TRANSLATING) &&
      tabbed_pane_->GetSelectedTabIndex() != 1) {
    // When switching to "after" or "during" translate view from something
    // other than user interaction, |this| needs to unregister from listening
    // to the tabbed pane events otherwise it'll trigger an additional
    // translation as if the user had clicked the tabs.
    tabbed_pane_->set_listener(nullptr);
    tabbed_pane_->SelectTabAt(1, false);
    tabbed_pane_->set_listener(this);
  } else if (view_state == TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE &&
             tabbed_pane_->GetSelectedTabIndex() != 0) {
    tabbed_pane_->SelectTabAt(0);
  }
}

void TranslateBubbleView::SwitchToErrorView(
    translate::TranslateErrors error_type) {
  SwitchView(TranslateBubbleModel::VIEW_STATE_ERROR);
  error_type_ = error_type;
  model_->ShowError(error_type);
}

void TranslateBubbleView::UpdateAdvancedView() {
  if (advanced_done_button_source_) {
    advanced_done_button_source_->SetText(
        l10n_util::GetStringUTF16(model_->IsPageTranslatedInCurrentLanguages()
                                      ? IDS_DONE
                                      : IDS_TRANSLATE_BUBBLE_ACCEPT));
    advanced_reset_button_source_->SetEnabled(DidLanguageSelectionChange(
        TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE));
  }
  if (advanced_done_button_target_) {
    advanced_done_button_target_->SetText(
        l10n_util::GetStringUTF16(model_->IsPageTranslatedInCurrentLanguages()
                                      ? IDS_DONE
                                      : IDS_TRANSLATE_BUBBLE_ACCEPT));
    advanced_reset_button_target_->SetEnabled(DidLanguageSelectionChange(
        TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE));
  }
  DeprecatedLayoutImmediately();
}

void TranslateBubbleView::UpdateLanguageNames(
    std::u16string* source_language_name,
    std::u16string* target_language_name) {
  DCHECK(source_language_name && target_language_name);
  previous_source_language_index_ = model_->GetSourceLanguageIndex();
  *source_language_name =
      model_->GetSourceLanguageNameAt(previous_source_language_index_);
  previous_target_language_index_ = model_->GetTargetLanguageIndex();
  *target_language_name =
      model_->GetTargetLanguageNameAt(previous_target_language_index_);
}

void TranslateBubbleView::UpdateInsets(TranslateBubbleModel::ViewState state) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL);

  gfx::Insets kTabStateMargins =
      gfx::Insets::TLBR(7, horizontal_spacing, 8, horizontal_spacing);
  gfx::Insets kDialogStateMargins =
      gfx::Insets::TLBR(2, horizontal_spacing, 16, horizontal_spacing);

  if (state == TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE ||
      state == TranslateBubbleModel::VIEW_STATE_TRANSLATING ||
      state == TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE) {
    set_margins(kTabStateMargins);
  } else {
    set_margins(kDialogStateMargins);
  }
}

void TranslateBubbleView::RevertOrDeclineTranslation() {
  if (model_->IsPageTranslatedInCurrentLanguages()) {
    model_->RevertTranslation();
  } else {
    model_->DeclineTranslation();
  }
  GetWidget()->Close();
}

BEGIN_METADATA(TranslateBubbleView)
END_METADATA
