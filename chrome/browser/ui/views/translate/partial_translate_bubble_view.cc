// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"

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
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/md_text_button_with_down_arrow.h"
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
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Container for |advanced_view_|. When the text on the "Translate"/"Done"
// button changes a layout is required.
class AdvancedViewContainer : public views::View {
 public:
  METADATA_HEADER(AdvancedViewContainer);
  AdvancedViewContainer() = default;
  AdvancedViewContainer(const AdvancedViewContainer&) = delete;
  AdvancedViewContainer& operator=(const AdvancedViewContainer&) = delete;

  void ChildPreferredSizeChanged(views::View* child) override { Layout(); }
};

BEGIN_METADATA(AdvancedViewContainer, views::View)
END_METADATA

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
  if (!model_->IsCurrentSelectionTranslated() && index == 1) {
    Translate();
  } else if (index == 0) {
    ShowOriginal();
  }
}

void PartialTranslateBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  translate_view_ = AddChildView(CreateView());
  advanced_view_source_ = AddChildView(CreateViewAdvancedSource());
  advanced_view_target_ = AddChildView(CreateViewAdvancedTarget());
  error_view_ = AddChildView(CreateViewError());

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));

  UpdateChildVisibilities();

  if (GetViewState() == PartialTranslateBubbleModel::VIEW_STATE_ERROR)
    model_->ShowError(error_type_);
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
        source_language_combobox_->GetSelectedIndex());
  } else {
    target_language_combobox_->SetSelectedIndex(
        previous_target_language_index_);
    model_->UpdateTargetLanguageIndex(
        target_language_combobox_->GetSelectedIndex());
  }
  UpdateAdvancedView();
}

bool PartialTranslateBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  switch (GetViewState()) {
    case PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        Translate();
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

gfx::Size PartialTranslateBubbleView::CalculatePreferredSize() const {
  int width = 0;
  for (const views::View* child : children())
    width = std::max(width, child->GetPreferredSize().width());
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
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_CHANGE_SOURCE_LANGUAGE,
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
      SwitchView(PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);
      break;

    case OptionsMenuItem::CHANGE_SOURCE_LANGUAGE:
      SwitchView(PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);
      break;

    default:
      NOTREACHED();
  }
}

void PartialTranslateBubbleView::OnWidgetClosing(views::Widget* widget) {
  // Nothing to do. When partial translate metrics get added we may want to log
  // when and how the bubble is closed similar to TranslateBubbleView.
  return;
}

PartialTranslateBubbleModel::ViewState
PartialTranslateBubbleView::GetViewState() const {
  return model_->GetViewState();
}

PartialTranslateBubbleView::PartialTranslateBubbleView(
    views::View* anchor_view,
    std::unique_ptr<PartialTranslateBubbleModel> model,
    translate::TranslateErrors::Type error_type,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      model_(std::move(model)),
      error_type_(error_type) {
  UpdateInsets(PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);

  if (web_contents)  // web_contents can be null in unit_tests.
    mouse_handler_ =
        std::make_unique<WebContentMouseHandler>(this, web_contents);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetFootnoteView(CreateWordmarkView());
  SetProperty(views::kElementIdentifierKey, kIdentifier);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::TRANSLATE);
}

views::View* PartialTranslateBubbleView::GetCurrentView() const {
  switch (GetViewState()) {
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
  return nullptr;
}

void PartialTranslateBubbleView::Translate() {
  // TODO(crbug/1314825): Update implementation when PartialTranslateManager is
  // complete.
  SwitchView(PartialTranslateBubbleModel::VIEW_STATE_TRANSLATING);
}

void PartialTranslateBubbleView::ShowOriginal() {
  // TODO(crbug/1314825): Update implementation when PartialTranslateManager is
  // complete.
  SwitchView(PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
}

void PartialTranslateBubbleView::ConfirmAdvancedOptions() {
  if (model_->IsCurrentSelectionTranslated()) {
    SwitchView(PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
    SizeToContents();
  } else {
    std::u16string source_language_name;
    std::u16string target_language_name;
    DCHECK(tabbed_pane_);
    UpdateLanguageNames(&source_language_name, &target_language_name);
    tabbed_pane_->GetTabAt(0)->SetTitleText(source_language_name);
    tabbed_pane_->GetTabAt(1)->SetTitleText(target_language_name);
    model_->Translate();
    tabbed_pane_->SelectTabAt(1);
    SwitchView(PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  }
}

void PartialTranslateBubbleView::SourceLanguageChanged() {
  model_->UpdateSourceLanguageIndex(
      source_language_combobox_->GetSelectedIndex());
  UpdateAdvancedView();
}

void PartialTranslateBubbleView::TargetLanguageChanged() {
  model_->UpdateTargetLanguageIndex(
      target_language_combobox_->GetSelectedIndex());
  UpdateAdvancedView();
}

void PartialTranslateBubbleView::UpdateChildVisibilities() {
  for (views::View* view : children())
    view->SetVisible(view == GetCurrentView());

  // BoxLayout only considers visible children, so ensure any newly visible
  // child views are positioned correctly.
  Layout();
}

std::unique_ptr<views::View> PartialTranslateBubbleView::CreateEmptyPane() {
  auto pane = std::make_unique<views::View>();
  pane->SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_DIALOG_SUBSECTION)));
  return pane;
}

std::unique_ptr<views::View> PartialTranslateBubbleView::CreateView() {
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

  // Desktop Partial Translate - placeholder button for switch to full page
  // translation button.
  auto button_row = std::make_unique<views::BoxLayoutView>();
  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  auto full_page_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](PartialTranslateBubbleModel* model) {
            // TODO(crbug/1314825): Update implementation when
            // PartialTranslateManager is
            // complete.
            model->Translate();
          },
          base::Unretained(model_.get())),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ACCEPT));
  button_row->AddChildView(std::move(full_page_button));
  button_row->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, 0,
                        provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  view->AddChildView(std::move(button_row));

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

  if (icon) {
    icon->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0,
                          provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
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
  options_menu->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, provider->GetDistanceMetric(
                             views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  return view;
}

// TODO(crbug/307350): Revise this later to show a specific message for each
// error.
std::unique_ptr<views::View> PartialTranslateBubbleView::CreateViewError() {
  auto translate_options_button =
      std::make_unique<views::MdTextButtonWithDownArrow>(
          views::Button::PressedCallback(),
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  translate_options_button->SetCallback(base::BindRepeating(
      &PartialTranslateBubbleView::ShowOptionsMenu, base::Unretained(this),
      base::Unretained(translate_options_button.get())));
  std::u16string translate_options_button_label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  translate_options_button->SetAccessibleName(translate_options_button_label);
  translate_options_button->SetTooltipText(translate_options_button_label);
  translate_options_button->SetRequestFocusOnPress(true);
  return CreateViewErrorNoTitle(std::move(translate_options_button));
}

std::unique_ptr<views::View> PartialTranslateBubbleView::CreateViewErrorNoTitle(
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
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  title_row->AddChildView(std::move(error_message_label));
  auto* close_button = title_row->AddChildView(CreateCloseButton());
  close_button->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);
  view->AddChildView(std::move(title_row));

  // Button row
  auto button_row = std::make_unique<views::BoxLayoutView>();
  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  button_row->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  auto try_again_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](PartialTranslateBubbleModel* model) { model->Translate(); },
          base::Unretained(model_.get())),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRY_AGAIN));
  button_row->AddChildView(std::move(try_again_button));
  button_row->AddChildView(std::move(advanced_button));
  button_row->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, 0,
                        provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  view->AddChildView(std::move(button_row));

  return view;
}

// TODO(cuianthony): The code for advanced view creation for
// PartialTranslateBubbleView is a duplicate of the code for
// TranslateBubbleView. This should be refactored to share the view creation
// code between the two bubbles.
std::unique_ptr<views::View>
PartialTranslateBubbleView::CreateViewAdvancedSource() {
  // Bubble title
  std::unique_ptr<views::Label> source_language_title_label =
      std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_SOURCE),
          views::style::CONTEXT_DIALOG_TITLE);

  // Language icon
  int source_default_index = model_->GetSourceLanguageIndex();
  source_language_combobox_model_ =
      std::make_unique<SourceLanguageComboboxModel>(source_default_index,
                                                    model_.get());
  auto source_language_combobox = std::make_unique<views::Combobox>(
      std::move(source_language_combobox_model_));
  source_language_combobox->SetProperty(views::kElementIdentifierKey,
                                        kSourceLanguageCombobox);

  source_language_combobox->SetCallback(
      base::BindRepeating(&PartialTranslateBubbleView::SourceLanguageChanged,
                          base::Unretained(this)));
  source_language_combobox->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_TRANSLATE_BUBBLE_SOURCE_LANG_COMBOBOX_ACCNAME));
  source_language_combobox_ = source_language_combobox.get();

  auto advanced_reset_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::ResetLanguage,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_RESET));
  advanced_reset_button_source_ = advanced_reset_button.get();

  auto advanced_done_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::ConfirmAdvancedOptions,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_DONE));
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
  target_language_combobox_model_ =
      std::make_unique<TargetLanguageComboboxModel>(target_default_index,
                                                    model_.get());

  auto target_language_combobox = std::make_unique<views::Combobox>(
      std::move(target_language_combobox_model_));
  target_language_combobox->SetProperty(views::kElementIdentifierKey,
                                        kTargetLanguageCombobox);

  target_language_combobox->SetCallback(
      base::BindRepeating(&PartialTranslateBubbleView::TargetLanguageChanged,
                          base::Unretained(this)));
  target_language_combobox->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_TRANSLATE_BUBBLE_TARGET_LANG_COMBOBOX_ACCNAME));
  target_language_combobox_ = target_language_combobox.get();

  auto advanced_reset_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::ResetLanguage,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_RESET));
  advanced_reset_button_target_ = advanced_reset_button.get();

  auto advanced_done_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PartialTranslateBubbleView::ConfirmAdvancedOptions,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_DONE));
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
  auto view = std::make_unique<AdvancedViewContainer>();
  auto* layout = view->SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_between_child_spacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

  std::unique_ptr<views::ImageView> language_icon = CreateTranslateIcon();
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  if (!UseGoogleTranslateBranding()) {
    // If the bottom branding isn't showing, display the leading translate
    // icon otherwise it's not obvious what the bubble is about. This should
    // only happen on non-Chrome-branded builds.
    auto* icon_view =
        view->AddChildView(std::make_unique<views::BoxLayoutView>());
    icon_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
    icon_view->AddChildView(std::move(language_icon));
    icon_view->SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(vertical_spacing, 0));
  }
  auto* form_view = view->AddChildView(std::make_unique<views::View>());
  form_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_spacing));

  language_title_label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(vertical_spacing, 0, vertical_spacing,
                        provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL) *
                            4));
  language_title_label->SetProperty(views::kCrossAxisAlignmentKey,
                                    views::LayoutAlignment::kStart);
  auto* title_row = form_view->AddChildView(std::make_unique<views::View>());
  title_row->SetLayoutManager(std::make_unique<views::FlexLayout>());
  title_row->AddChildView(std::move(language_title_label));
  title_row->AddChildView(CreateCloseButton())
      ->SetProperty(views::kCrossAxisAlignmentKey,
                    views::LayoutAlignment::kStart);

  form_view->AddChildView(std::move(combobox))
      ->SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(0, 0, 0,
                            provider->GetDistanceMetric(
                                views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  auto button_row = std::make_unique<views::BoxLayoutView>();
  button_row->SetProperty(views::kMarginsKey,
                          gfx::Insets::TLBR(2 * vertical_spacing, 0, 0, 0));

  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  button_row->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  button_row->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, 0,
                        provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
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
  gfx::ImageSkia* language_icon_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          language_icon_id);
  language_icon->SetImage(*language_icon_image);
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
  tab_translate_options_button->SetAccessibleName(
      translate_options_button_label);
  tab_translate_options_button->SetTooltipText(translate_options_button_label);
  tab_translate_options_button->SetRequestFocusOnPress(true);
  tab_translate_options_button->SetVisible(true);
  return tab_translate_options_button;
}

std::unique_ptr<views::Button> PartialTranslateBubbleView::CreateCloseButton() {
  auto close_button =
      views::BubbleFrameView::CreateCloseButton(base::BindRepeating(
          [](View* view) {
            view->GetWidget()->CloseWithReason(
                views::Widget::ClosedReason::kCloseButtonClicked);
          },
          base::Unretained(this)));
  close_button->SetProperty(views::kElementIdentifierKey, kCloseButton);
  close_button->SetVisible(true);
  return close_button;
}

void PartialTranslateBubbleView::SetWindowTitle(
    PartialTranslateBubbleModel::ViewState view_state) {
  switch (view_state) {
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
      SetTitle(IDS_TRANSLATE_BUBBLE_ADVANCED_SOURCE);
      break;
    case PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE:
      SetTitle(IDS_TRANSLATE_BUBBLE_ADVANCED_TARGET);
      break;
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

  if (view_state == PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
      view_state == PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
    GetBubbleFrameView()->SetFootnoteView(nullptr);
  } else {
    GetBubbleFrameView()->SetFootnoteView(CreateWordmarkView());
  }

  SwitchTabForViewState(view_state);

  UpdateViewState(view_state);
  if (view_state == PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
      view_state == PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE)
    UpdateAdvancedView();

  UpdateChildVisibilities();
  SizeToContents();

  if (view_state == PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE) {
    GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_BUBBLE_TRANSLATION_COMPLETE_ANNOUNCEMENT,
        model_->GetTargetLanguageNameAt(model_->GetTargetLanguageIndex())));
  } else if (view_state == PartialTranslateBubbleModel::VIEW_STATE_ERROR) {
    GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
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
    translate::TranslateErrors::Type error_type) {
  SwitchView(PartialTranslateBubbleModel::VIEW_STATE_ERROR);
  error_type_ = error_type;
  model_->ShowError(error_type);
}

void PartialTranslateBubbleView::UpdateAdvancedView() {
  if (advanced_done_button_source_) {
    advanced_done_button_source_->SetText(l10n_util::GetStringUTF16(
        model_->IsCurrentSelectionTranslated() ? IDS_DONE
                                               : IDS_TRANSLATE_BUBBLE_ACCEPT));
    advanced_reset_button_source_->SetEnabled(DidLanguageSelectionChange(
        PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE));
  }
  if (advanced_done_button_target_) {
    advanced_done_button_target_->SetText(l10n_util::GetStringUTF16(
        model_->IsCurrentSelectionTranslated() ? IDS_DONE
                                               : IDS_TRANSLATE_BUBBLE_ACCEPT));
    advanced_reset_button_target_->SetEnabled(DidLanguageSelectionChange(
        PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE));
  }
  Layout();
}

void PartialTranslateBubbleView::UpdateLanguageNames(
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

void PartialTranslateBubbleView::UpdateInsets(
    PartialTranslateBubbleModel::ViewState state) {
  gfx::Insets kTabStateMargins = gfx::Insets::TLBR(7, 16, 8, 12);
  gfx::Insets kDialogStateMargins = gfx::Insets::TLBR(5, 16, 16, 4);

  if (state == PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE ||
      state == PartialTranslateBubbleModel::VIEW_STATE_TRANSLATING ||
      state == PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE) {
    set_margins(kTabStateMargins);
  } else {
    set_margins(kDialogStateMargins);
  }
}
