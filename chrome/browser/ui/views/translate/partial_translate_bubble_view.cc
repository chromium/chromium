// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
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
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_ui_action_logger.h"
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
#include "ui/gfx/text_constants.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
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

}  // namespace

PartialTranslateBubbleView::~PartialTranslateBubbleView() {
  // A child view could refer to a model which is owned by this class when
  // the child view is destructed. For example, |source_language_combobx_model_|
  // is referred by Combobox's destructor. Before destroying the models,
  // removing the child views is needed.
  RemoveAllChildViews();
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView, kIdentifier);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kSourceLanguageTab);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kTargetLanguageTab);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView, kCloseButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kOptionsMenuButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kChangeTargetLanguage);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kTargetLanguageCombobox);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kTargetLanguageDoneButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kChangeSourceLanguage);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kSourceLanguageCombobox);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kSourceLanguageDoneButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PartialTranslateBubbleView,
                                      kErrorMessage);

void PartialTranslateBubbleView::CloseBubble() {
  mouse_handler_.reset();
  LocationBarBubbleDelegateView::CloseBubble();
}

void PartialTranslateBubbleView::TabSelectedAt(int index) {
  // Tabbed pane is indexed from left to right starting at 0.
  switch (index) {
    case 0:
      ShowOriginal();
      break;
    case 1:
      ShowTranslated();
      break;
    default:
      NOTREACHED();
  }
}

void PartialTranslateBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  translate_view_ = AddChildView(CreateView());
  advanced_view_source_ = AddChildView(CreateViewAdvancedSource());
  advanced_view_target_ = AddChildView(CreateViewAdvancedTarget());
  error_view_ = AddChildView(CreateViewError());
  // NOTE: The waiting view should be added last to avoid it having default
  // focus when shown.
  translate_view_waiting_ = AddChildView(CreateViewWaiting());
  ComputeLargestViewStateWidth();

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));

  UpdateChildVisibilities();
}

views::View* PartialTranslateBubbleView::GetInitiallyFocusedView() {
  return GetCurrentView()->GetNextFocusableView();
}

bool PartialTranslateBubbleView::ShouldShowCloseButton() const {
  return false;
}

bool PartialTranslateBubbleView::ShouldShowWindowTitle() const {
  return false;
}

bool PartialTranslateBubbleView::DidLanguageSelectionChange(
    PartialTranslateBubbleModel::ViewState view_state) {
  if (view_state == PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE) {
    return source_language_combobox_->GetSelectedIndex() !=
           previous_source_language_index_;
  } else {
    return target_language_combobox_->GetSelectedIndex() !=
           previous_target_language_index_;
  }
}

void PartialTranslateBubbleView::ResetLanguage() {
  if (GetViewState() ==
      PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE) {
    source_language_combobox_->SetSelectedIndex(
        previous_source_language_index_);
    model_->UpdateSourceLanguageIndex(
        source_language_combobox_->GetSelectedIndex().value());
    translate::ReportPartialTranslateBubbleUiAction(
        translate::PartialTranslateBubbleUiEvent::
            SOURCE_LANGUAGE_RESET_BUTTON_CLICKED);
  } else {
    target_language_combobox_->SetSelectedIndex(
        previous_target_language_index_);
    model_->UpdateTargetLanguageIndex(
        target_language_combobox_->GetSelectedIndex().value());
    translate::ReportPartialTranslateBubbleUiAction(
        translate::PartialTranslateBubbleUiEvent::
            TARGET_LANGUAGE_RESET_BUTTON_CLICKED);
  }
  UpdateAdvancedView();
}

void PartialTranslateBubbleView::WindowClosing() {
  // We have to reset the controller reference to the view here, not in our
  // destructor, because we'll be destroyed asynchronously and the shown state
  // will be checked before then.
  if (on_closing_) {
    std::move(on_closing_).Run();
  }
}

bool PartialTranslateBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  switch (GetViewState()) {
    case PartialTranslateBubbleModel::VIEW_STATE_WAITING:
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        ShowTranslated();
        return true;
      }
      break;
    }
    case PartialTranslateBubbleModel::VIEW_STATE_TRANSLATING:
    case PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        ShowOriginal();
        return true;
      }
      break;
    }
    case PartialTranslateBubbleModel::VIEW_STATE_ERROR:
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE:
      break;
  }
  return BubbleDialogDelegateView::AcceleratorPressed(accelerator);
}

gfx::Size PartialTranslateBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int width = 0;
  for (const views::View* child : children()) {
    width = std::max(width, child->GetPreferredSize().width());
  }
  return gfx::Size(width, GetCurrentView()->GetPreferredSize().height());
}

// Create the menu items for the dropdown options menu under TAB UI.
void PartialTranslateBubbleView::ShowOptionsMenu(views::Button* source) {
  // Recreate the menu model as translated languages can change while the menu
  // is not showing, which invalidates these text strings.
  options_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  options_menu_model_->AddItemWithStringId(
      OptionsMenuItem::CHANGE_TARGET_LANGUAGE,
      IDS_TRANSLATE_BUBBLE_CHANGE_TARGET_LANGUAGE);
  options_menu_model_->SetElementIdentifierAt(
      options_menu_model_->GetItemCount() - 1, kChangeTargetLanguage);

  auto source_language =
      model_->GetSourceLanguageNameAt(model_->GetSourceLanguageIndex());
  options_menu_model_->AddItem(
      OptionsMenuItem::CHANGE_SOURCE_LANGUAGE,
      l10n_util::GetStringFUTF16(
          IDS_PARTIAL_TRANSLATE_BUBBLE_CHANGE_SOURCE_LANGUAGE,
          source_language));
  options_menu_model_->SetElementIdentifierAt(
      options_menu_model_->GetItemCount() - 1, kChangeSourceLanguage);

  options_menu_runner_ = std::make_unique<views::MenuRunner>(
      options_menu_model_.get(), views::MenuRunner::COMBOBOX);
  gfx::Rect screen_bounds = source->GetAnchorBoundsInScreen();
  options_menu_runner_->RunMenuAt(source->GetWidget(), nullptr, screen_bounds,
                                  views::MenuAnchorPosition::kTopRight,
                                  ui::MENU_SOURCE_MOUSE);
}

void PartialTranslateBubbleView::ExecuteCommand(int command_id,
                                                int event_flags) {
  switch (command_id) {
    case OptionsMenuItem::CHANGE_TARGET_LANGUAGE:
      translate::ReportPartialTranslateBubbleUiAction(
          translate::PartialTranslateBubbleUiEvent::
              CHANGE_TARGET_LANGUAGE_OPTION_CLICKED);
      SwitchView(PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);
      break;

    case OptionsMenuItem::CHANGE_SOURCE_LANGUAGE:
      translate::ReportPartialTranslateBubbleUiAction(
          translate::PartialTranslateBubbleUiEvent::
              CHANGE_SOURCE_LANGUAGE_OPTION_CLICKED);
      SwitchView(PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);
      break;

    default:
      NOTREACHED();
  }
}

void PartialTranslateBubbleView::OnWidgetDestroying(views::Widget* widget) {
  // Nothing to do. When Partial Translate metrics get added we may want to log
  // when and how the bubble is closed similar to TranslateBubbleView.
  return;
}

PartialTranslateBubbleModel::ViewState
PartialTranslateBubbleView::GetViewState() const {
  return model_->GetViewState();
}

void PartialTranslateBubbleView::SetViewState(
    PartialTranslateBubbleModel::ViewState view_state,
    translate::TranslateErrors error_type) {
  if (view_state == PartialTranslateBubbleModel::VIEW_STATE_ERROR) {
    SwitchToErrorView(error_type);
  } else {
    SwitchView(view_state);
  }
}

PartialTranslateBubbleView::PartialTranslateBubbleView(
    views::View* anchor_view,
    std::unique_ptr<PartialTranslateBubbleModel> model,
    content::WebContents* web_contents,
    base::OnceClosure on_closing)
    : LocationBarBubbleDelegateView(anchor_view,
                                    web_contents,
                                    /*autosize=*/true),
      model_(std::move(model)),
      on_closing_(std::move(on_closing)),
      web_contents_(web_contents) {
  UpdateInsets(PartialTranslateBubbleModel::VIEW_STATE_WAITING);

  previous_source_language_index_ = model_->GetSourceLanguageIndex();
  previous_target_language_index_ = model_->GetTargetLanguageIndex();

  if (web_contents) {  // web_contents can be null in unit_tests.
    mouse_handler_ =
        std::make_unique<WebContentMouseHandler>(this, web_contents);
  }
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetFootnoteView(CreateWordmarkView());
  SetProperty(views::kElementIdentifierKey, kIdentifier);
}

views::View* PartialTranslateBubbleView::GetCurrentView() const {
  switch (GetViewState()) {
    case PartialTranslateBubbleModel::VIEW_STATE_WAITING:
      return translate_view_waiting_;
    case PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE:
      return translate_view_;
    case PartialTranslateBubbleModel::VIEW_STATE_TRANSLATING:
      return translate_view_;
    case PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE:
      return translate_view_;
    case PartialTranslateBubbleModel::VIEW_STATE_ERROR:
      return error_view_;
    case PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE:
      return advanced_view_source_;
    case PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      return advanced_view_target_;
  }
  NOTREACHED();
}

void PartialTranslateBubbleView::ShowTranslated() {
  SwitchView(PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  translate::ReportPartialTranslateBubbleUiAction(
      translate::PartialTranslateBubbleUiEvent::TARGET_LANGUAGE_TAB_SELECTED);
}

void PartialTranslateBubbleView::ShowOriginal() {
  SwitchView(PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
  translate::ReportPartialTranslateBubbleUiAction(
      translate::PartialTranslateBubbleUiEvent::SOURCE_LANGUAGE_TAB_SELECTED);
}

void PartialTranslateBubbleView::ConfirmAdvancedOptions() {
  // This call is either made from the source or target language selection view.
  // Save the view state we're coming from so that the correct ui action can be
  // logged.
  bool from_source_language_view =
      GetViewState() == PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE;
  if (DidLanguageSelectionChange(
          PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE) ||
      DidLanguageSelectionChange(
          PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE)) {
    previous_source_language_index_ = model_->GetSourceLanguageIndex();
    previous_target_language_index_ = model_->GetTargetLanguageIndex();
    UpdateLanguageTabNames();
    model_->Translate(web_contents_);
    target_language_changed_ = true;
    SwitchView(PartialTranslateBubbleModel::VIEW_STATE_WAITING);
    if (from_source_language_view) {
      translate::ReportPartialTranslateBubbleUiAction(
          translate::PartialTranslateBubbleUiEvent::
              SOURCE_LANGUAGE_SELECTION_TRANSLATE_BUTTON_CLICKED);
    } else {
      translate::ReportPartialTranslateBubbleUiAction(
          translate::PartialTranslateBubbleUiEvent::
              TARGET_LANGUAGE_SELECTION_TRANSLATE_BUTTON_CLICKED);
    }
  } else {
    SwitchView(PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
    if (from_source_language_view) {
      translate::ReportPartialTranslateBubbleUiAction(
          translate::PartialTranslateBubbleUiEvent::
              SOURCE_LANGUAGE_SELECTION_DONE_BUTTON_CLICKED);
    } else {
      translate::ReportPartialTranslateBubbleUiAction(
          translate::PartialTranslateBubbleUiEvent::
              TARGET_LANGUAGE_SELECTION_DONE_BUTTON_CLICKED);
    }
  }
}

void PartialTranslateBubbleView::UpdateLanguageTabNames() {
  DCHECK(tabbed_pane_);
  tabbed_pane_->GetTabAt(0)->SetTitleText(model_->GetSourceLanguageName());
  tabbed_pane_->GetTabAt(1)->SetTitleText(model_->GetTargetLanguageName());
}

void PartialTranslateBubbleView::SourceLanguageChanged() {
  model_->UpdateSourceLanguageIndex(
      source_language_combobox_->GetSelectedIndex().value());
  UpdateAdvancedView();
  translate::ReportPartialTranslateBubbleUiAction(
      translate::PartialTranslateBubbleUiEvent::
          SOURCE_LANGUAGE_MENU_ITEM_CLICKED);
}

void PartialTranslateBubbleView::TargetLanguageChanged() {
  model_->UpdateTargetLanguageIndex(
      target_language_combobox_->GetSelectedIndex().value());
  UpdateAdvancedView();
  translate::ReportPartialTranslateBubbleUiAction(
      translate::PartialTranslateBubbleUiEvent::
          TARGET_LANGUAGE_MENU_ITEM_CLICKED);
}

void PartialTranslateBubbleView::UpdateChildVisibilities() {
  for (views::View* view : children()) {
    view->SetVisible(view == GetCurrentView());
  }
}

std::unique_ptr<views::View> PartialTranslateBubbleView::CreateEmptyPane() {
  auto pane = std::make_unique<views::View>();
  pane->SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_DIALOG_SUBSECTION)));
  return pane;
}

std::unique_ptr<views::View> PartialTranslateBubbleView::CreateView() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  auto inner_view = std::make_unique<views::View>();
  inner_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
  tab_view_top_row_ = view->AddChildView(std::move(inner_view));

  views::View* icon = nullptr;
  if (!UseGoogleTranslateBranding()) {
    icon = tab_view_top_row_->AddChildView(CreateTranslateIcon());
  }

  // Tabbed pane for language selection. Can't use unique_ptr because
  // tabs have to be added after the tabbed_pane is added to the parent,
  // when we release ownership of the unique_ptr.
  auto tabbed_pane = std::make_unique<views::TabbedPane>();
  tabbed_pane_ = tab_view_top_row_->AddChildView(std::move(tabbed_pane));

  // NOTE: Panes must be added after |tabbed_pane| has been added to its
  // parent.
  tabbed_pane_->AddTab(model_->GetSourceLanguageName(), CreateEmptyPane());
  tabbed_pane_->AddTab(model_->GetTargetLanguageName(), CreateEmptyPane());
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
      tab_view_top_row_->AddChildView(std::make_unique<views::View>());
  auto* options_menu =
      tab_view_top_row_->AddChildView(CreateOptionsMenuButton());
  tab_view_top_row_->AddChildView(CreateCloseButton());

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
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2));
  options_menu->SetProperty(views::kElementIdentifierKey, kOptionsMenuButton);
  options_menu->SetProperty(views::kMarginsKey,
                            gfx::Insets::VH(0, button_horizontal_spacing));

  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);

  // Text selection.
  auto partial_text_label = std::make_unique<views::Label>(
      model_->GetSourceText(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  partial_text_label->SetMultiLine(true);
  partial_text_label->SizeToFit(tab_view_top_row_->GetPreferredSize().width());
  partial_text_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  partial_text_label->SetProperty(views::kMarginsKey,
                                  gfx::Insets::VH(vertical_spacing, 0));
  partial_text_label_ = view->AddChildView(std::move(partial_text_label));
  SetTextAlignmentForLocaleTextDirection(model_->GetSourceLanguageCode());

  // Button to trigger full page translation.
  auto button_row = std::make_unique<views::BoxLayoutView>();
  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  auto full_page_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::TranslateFullPage,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_PARTIAL_TRANSLATE_BUBBLE_TRANSLATE_FULL_PAGE));
  full_page_button->SetID(BUTTON_ID_FULL_PAGE_TRANSLATE);
  button_row->AddChildView(std::move(full_page_button));
  button_row->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_bottom(vertical_spacing));
  view->AddChildView(std::move(button_row));

  return view;
}

// TODO(crbug.com/40335010): Revise this later to show a specific message for
// each error.
std::unique_ptr<views::View> PartialTranslateBubbleView::CreateViewError() {
  auto full_page_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::TranslateFullPage,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_PARTIAL_TRANSLATE_BUBBLE_TRANSLATE_FULL_PAGE));
  full_page_button->SetID(BUTTON_ID_FULL_PAGE_TRANSLATE);
  return CreateViewErrorNoTitle(std::move(full_page_button));
}

std::unique_ptr<views::View> PartialTranslateBubbleView::CreateViewErrorNoTitle(
    std::unique_ptr<views::Button> button) {
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
  int error_message_id = IDS_PARTIAL_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE;
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
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  title_row->AddChildView(std::move(error_message_label));
  title_row->AddChildView(CreateCloseButton());
  view->AddChildView(std::move(title_row));

  // Button row
  auto button_row = std::make_unique<views::BoxLayoutView>();
  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  button_row->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  auto try_again_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](PartialTranslateBubbleModel* model,
             content::WebContents* web_contents) {
            translate::ReportPartialTranslateBubbleUiAction(
                translate::PartialTranslateBubbleUiEvent::
                    TRY_AGAIN_BUTTON_CLICKED);
            model->Translate(web_contents);
          },
          model_.get(), web_contents_),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRY_AGAIN));
  try_again_button->SetID(BUTTON_ID_TRY_AGAIN);
  button_row->AddChildView(std::move(try_again_button));
  button_row->AddChildView(std::move(button));
  view->AddChildView(std::move(button_row));

  return view;
}

std::unique_ptr<views::View> PartialTranslateBubbleView::CreateViewWaiting() {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  auto view = std::make_unique<views::View>();
  views::BoxLayout* layout =
      view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_between_child_spacing(vertical_spacing);

  // Title row.
  auto close_button_container = std::make_unique<views::BoxLayoutView>();
  close_button_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  auto* close_button =
      close_button_container->AddChildView(CreateCloseButton());
  // The positioning of the close button should match that of the same button
  // in the Tab UI. However, the button in the Tab UI is uniquely spaced due to
  // its layout behaviour with the views around it. Ideally the margins here
  // would not be hard-coded but they are needed to match the spacing.
  close_button->SetProperty(views::kMarginsKey, gfx::Insets().set_top(9));
  view->AddChildView(std::move(close_button_container));

  const int throbber_diameter = 35;
  auto throbber_container = std::make_unique<views::FlexLayoutView>();
  throbber_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  auto throbber = std::make_unique<views::Throbber>();
  throbber->SetPreferredSize(gfx::Size(throbber_diameter, throbber_diameter));
  throbber->SetProperty(views::kMarginsKey,
                        gfx::Insets().set_bottom(vertical_spacing));
  throbber_ = throbber_container->AddChildView(std::move(throbber));
  view->AddChildView(std::move(throbber_container));

  return view;
}

// TODO(cuianthony): The code for advanced view creation for
// PartialTranslateBubbleView is a near duplicate of the code for
// TranslateBubbleView. This should be refactored to share the view creation
// code between the two bubbles.
std::unique_ptr<views::View>
PartialTranslateBubbleView::CreateViewAdvancedSource() {
  // Bubble title
  std::unique_ptr<views::Label> source_language_title_label =
      std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(
              IDS_PARTIAL_TRANSLATE_BUBBLE_ADVANCED_SOURCE),
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

  source_language_combobox->SetCallback(
      base::BindRepeating(&PartialTranslateBubbleView::SourceLanguageChanged,
                          base::Unretained(this)));
  source_language_combobox->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_TRANSLATE_BUBBLE_SOURCE_LANG_COMBOBOX_ACCNAME));
  source_language_combobox_ = source_language_combobox.get();

  auto advanced_reset_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::ResetLanguage,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_RESET));
  advanced_reset_button->SetID(BUTTON_ID_RESET);
  advanced_reset_button_source_ = advanced_reset_button.get();

  auto advanced_done_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::ConfirmAdvancedOptions,
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
                            std::move(advanced_done_button));
}

std::unique_ptr<views::View>
PartialTranslateBubbleView::CreateViewAdvancedTarget() {
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

  target_language_combobox->SetCallback(
      base::BindRepeating(&PartialTranslateBubbleView::TargetLanguageChanged,
                          base::Unretained(this)));
  target_language_combobox->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_TRANSLATE_BUBBLE_TARGET_LANG_COMBOBOX_ACCNAME));
  target_language_combobox_ = target_language_combobox.get();

  auto advanced_reset_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::ResetLanguage,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_RESET));
  advanced_reset_button->SetID(BUTTON_ID_RESET);
  advanced_reset_button_target_ = advanced_reset_button.get();

  auto advanced_done_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::ConfirmAdvancedOptions,
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
                            std::move(advanced_done_button));
}

std::unique_ptr<views::View> PartialTranslateBubbleView::CreateViewAdvanced(
    std::unique_ptr<views::Combobox> combobox,
    std::unique_ptr<views::Label> language_title_label,
    std::unique_ptr<views::Button> advanced_reset_button,
    std::unique_ptr<views::Button> advanced_done_button) {
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
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2));

  form_view->AddChildView(std::move(combobox));

  auto button_row = std::make_unique<views::BoxLayoutView>();
  button_row->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_top(2 * vertical_spacing));
  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  button_row->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  button_row->AddChildView(std::move(advanced_reset_button));
  button_row->AddChildView(std::move(advanced_done_button));
  form_view->AddChildView(std::move(button_row));

  UpdateAdvancedView();

  return view;
}

std::unique_ptr<views::ImageView>
PartialTranslateBubbleView::CreateTranslateIcon() {
  const int language_icon_id = IDR_TRANSLATE_BUBBLE_ICON;
  std::unique_ptr<views::ImageView> language_icon =
      std::make_unique<views::ImageView>();
  language_icon->SetImage(ui::ImageModel::FromResourceId(language_icon_id));
  return language_icon;
}

std::unique_ptr<views::Button>
PartialTranslateBubbleView::CreateOptionsMenuButton() {
  // Three dots options menu button
  auto tab_translate_options_button =
      views::CreateVectorImageButtonWithNativeTheme(
          views::Button::PressedCallback(), kBrowserToolsIcon);
  tab_translate_options_button->SetCallback(base::BindRepeating(
      &PartialTranslateBubbleView::ShowOptionsMenu, base::Unretained(this),
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

std::unique_ptr<views::Button> PartialTranslateBubbleView::CreateCloseButton() {
  auto close_button =
      views::BubbleFrameView::CreateCloseButton(base::BindRepeating(
          [](View* view) {
            translate::ReportPartialTranslateBubbleUiAction(
                translate::PartialTranslateBubbleUiEvent::CLOSE_BUTTON_CLICKED);
            view->GetWidget()->CloseWithReason(
                views::Widget::ClosedReason::kCloseButtonClicked);
          },
          base::Unretained(this)));
  close_button->SetProperty(views::kElementIdentifierKey, kCloseButton);
  close_button->SetVisible(true);
  close_button->SetID(BUTTON_ID_CLOSE);
  return close_button;
}

void PartialTranslateBubbleView::SetWindowTitle(
    PartialTranslateBubbleModel::ViewState view_state) {
  switch (view_state) {
    case PartialTranslateBubbleModel::VIEW_STATE_WAITING:
      SetTitle(IDS_PARTIAL_TRANSLATE_BUBBLE_WAITING_TITLE);
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE:
      SetTitle(IDS_TRANSLATE_BUBBLE_BEFORE_TRANSLATE_TITLE);
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_TRANSLATING:
      SetTitle(IDS_TRANSLATE_BUBBLE_TRANSLATING);
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE:
      SetTitle(IDS_TRANSLATE_BUBBLE_TRANSLATED_TITLE);
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_ERROR:
      SetTitle(IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE);
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE:
      SetTitle(IDS_PARTIAL_TRANSLATE_BUBBLE_ADVANCED_SOURCE);
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      SetTitle(IDS_TRANSLATE_BUBBLE_ADVANCED_TARGET);
      break;
  }
}

void PartialTranslateBubbleView::ComputeLargestViewStateWidth() {
  for (views::View* view : children()) {
    if (view == translate_view_) {
      continue;
    }
    int width = view->GetPreferredSize().width();
    if (width > largest_view_state_width_) {
      largest_view_state_width_ = width;
    }
  }
}

void PartialTranslateBubbleView::UpdateViewState(
    PartialTranslateBubbleModel::ViewState view_state) {
  model_->SetViewState(view_state);
  SetWindowTitle(view_state);
}

void PartialTranslateBubbleView::SwitchView(
    PartialTranslateBubbleModel::ViewState view_state) {
  UpdateInsets(view_state);

  SwitchTabForViewState(view_state);
  // The initial partial translation uses "Detected Language" as the source by
  // default, so |partial_text_label_| needs to be resized after receiving the
  // actual source language string. This is done in UpdateTextForViewState.
  UpdateLanguageTabNames();
  UpdateTextForViewState(view_state);

  // In cases where we are switching from the waiting view, the spinner should
  // be stopped.
  if (throbber_) {
    if (view_state == PartialTranslateBubbleModel::VIEW_STATE_WAITING) {
      throbber_->Start();
    } else {
      throbber_->Stop();
    }
  }

  UpdateViewState(view_state);
  if (view_state == PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
      view_state == PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
    UpdateAdvancedView();
    GetBubbleFrameView()->SetFootnoteView(nullptr);
  } else {
    GetBubbleFrameView()->SetFootnoteView(CreateWordmarkView());
  }

  UpdateChildVisibilities();
}

void PartialTranslateBubbleView::UpdateTextForViewState(
    PartialTranslateBubbleModel::ViewState view_state) {
  if (view_state == PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE ||
      view_state == PartialTranslateBubbleModel::VIEW_STATE_TRANSLATING) {
    partial_text_label_->SetText(model_->GetTargetText());
    SetTextAlignmentForLocaleTextDirection(model_->GetTargetLanguageCode());
  } else if (view_state ==
             PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) {
    partial_text_label_->SetText(model_->GetSourceText());
    SetTextAlignmentForLocaleTextDirection(model_->GetSourceLanguageCode());
  }

  // Use the maximum set width for the bubble for the largest text volumes to
  // prevent sizing of the bubble that exceeds screen height.
  if (partial_text_label_->GetText().length() > char_threshold_for_max_width_) {
    partial_text_label_->SizeToFit(bubble_max_width_);
  } else {
    // Otherwise, with no risk of overflow, resize the text label to match the
    // width of the bubble. This will depend on either the preferred width of
    // the tabbed pane, or |largest_view_state_width_|, which serves as a lower
    // bound.
    if (tab_view_top_row_->GetPreferredSize().width() <
        largest_view_state_width_) {
      partial_text_label_->SizeToFit(largest_view_state_width_);
    } else {
      partial_text_label_->SizeToFit(
          tab_view_top_row_->GetPreferredSize().width());
    }
  }

  AnnounceForAccessibility(view_state);
}

void PartialTranslateBubbleView::MaybeUpdateSourceLanguageCombobox() {
  size_t curr_index = model_->GetSourceLanguageIndex();
  if (source_language_combobox_->GetSelectedIndex() != curr_index) {
    source_language_combobox_->SetSelectedIndex(curr_index);
    previous_source_language_index_ = curr_index;
  }
}

void PartialTranslateBubbleView::AnnounceForAccessibility(
    PartialTranslateBubbleModel::ViewState view_state) {
  if (view_state == PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE) {
    std::u16string base_text = l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_BUBBLE_TRANSLATION_COMPLETE_ANNOUNCEMENT,
        model_->GetTargetLanguageNameAt(model_->GetTargetLanguageIndex()));
    // "," seems to be working better than "." to separate announcements,
    // especially for selections that are not full sentences.
    std::u16string full_text = l10n_util::GetStringFUTF16(
        IDS_CONCAT_TWO_STRINGS_WITH_COMMA, base_text, model_->GetTargetText());

    if (target_language_changed_) {
      partial_text_label_->GetViewAccessibility().AnnounceText(full_text);
    } else {
      partial_text_label_->GetViewAccessibility().AnnounceText(base_text);
    }
  } else if (view_state == PartialTranslateBubbleModel::VIEW_STATE_ERROR) {
    partial_text_label_->GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(
            IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE));
  }
}
void PartialTranslateBubbleView::SwitchTabForViewState(
    PartialTranslateBubbleModel::ViewState view_state) {
  if ((view_state == PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE ||
       view_state == PartialTranslateBubbleModel::VIEW_STATE_TRANSLATING) &&
      tabbed_pane_->GetSelectedTabIndex() != 1) {
    // When switching to "after" or "during" translate view from something
    // other than user interaction, |this| needs to unregister from listening
    // to the tabbed pane events otherwise it'll trigger an additional
    // translation as if the user had clicked the tabs.
    tabbed_pane_->set_listener(nullptr);
    tabbed_pane_->SelectTabAt(1, false);
    tabbed_pane_->set_listener(this);
  } else if (view_state ==
                 PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE &&
             tabbed_pane_->GetSelectedTabIndex() != 0) {
    tabbed_pane_->SelectTabAt(0);
  }
}

void PartialTranslateBubbleView::SwitchToErrorView(
    translate::TranslateErrors error_type) {
  SwitchView(PartialTranslateBubbleModel::VIEW_STATE_ERROR);
  model_->SetError(error_type);
  // TODO(crbug.com/40221545): Record the error when Partial Translate-specific
  // metrics are added.
}

void PartialTranslateBubbleView::UpdateAdvancedView() {
  if (advanced_done_button_source_) {
    bool changed = DidLanguageSelectionChange(
        PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);
    advanced_done_button_source_->SetText(l10n_util::GetStringUTF16(
        changed ? IDS_TRANSLATE_BUBBLE_ACCEPT : IDS_DONE));
    advanced_reset_button_source_->SetEnabled(changed);
  }
  if (advanced_done_button_target_) {
    bool changed = DidLanguageSelectionChange(
        PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);
    advanced_done_button_target_->SetText(l10n_util::GetStringUTF16(
        changed ? IDS_TRANSLATE_BUBBLE_ACCEPT : IDS_DONE));
    advanced_reset_button_target_->SetEnabled(changed);
  }
  DeprecatedLayoutImmediately();
}

void PartialTranslateBubbleView::UpdateInsets(
    PartialTranslateBubbleModel::ViewState state) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL);

  // The constants used for insets serve two purposes. |horizontal_spacing| is
  // used as the UX standard for horizontal padding on the edges. Vertical
  // insets keep top-most elements as closely aligned as possible across the
  // bubble's different view states. Some additional margins are set in view
  // construction due to small differences in how elements are laid out.
  gfx::Insets kTabStateMargins =
      gfx::Insets::TLBR(7, horizontal_spacing, 8, horizontal_spacing);
  gfx::Insets kDialogStateMargins =
      gfx::Insets::TLBR(2, horizontal_spacing, 16, horizontal_spacing);

  if (state == PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE ||
      state == PartialTranslateBubbleModel::VIEW_STATE_TRANSLATING ||
      state == PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE) {
    set_margins(kTabStateMargins);
  } else {
    set_margins(kDialogStateMargins);
  }
}

void PartialTranslateBubbleView::TranslateFullPage() {
  translate::ReportPartialTranslateBubbleUiAction(
      translate::PartialTranslateBubbleUiEvent::
          TRANSLATE_FULL_PAGE_BUTTON_CLICKED);
  model_.get()->TranslateFullPage(web_contents_);
}

void PartialTranslateBubbleView::SetTextAlignmentForLocaleTextDirection(
    std::string locale) {
  base::i18n::TextDirection direction =
      base::i18n::GetTextDirectionForLocale(locale.c_str());
  if (direction == base::i18n::TextDirection::LEFT_TO_RIGHT) {
    partial_text_label_->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
  } else {
    partial_text_label_->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_RIGHT);
  }
}

BEGIN_METADATA(PartialTranslateBubbleView)
END_METADATA
