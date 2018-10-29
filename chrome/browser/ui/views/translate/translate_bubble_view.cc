// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include <stddef.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/string_compare.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

namespace {

// Container for |advanced_view_|. When the text on the "Translate"/"Done"
// button changes a layout is required.
class AdvancedViewContainer : public views::View {
 public:
  AdvancedViewContainer() {}

  void ChildPreferredSizeChanged(views::View* child) override { Layout(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(AdvancedViewContainer);
};

}  // namespace

// static
TranslateBubbleView* TranslateBubbleView::translate_bubble_view_ = NULL;

TranslateBubbleView::~TranslateBubbleView() {
  // A child view could refer to a model which is owned by this class when
  // the child view is destructed. For example, |source_language_combobx_model_|
  // is referred by Combobox's destructor. Before destroying the models,
  // removing the child views is needed.
  RemoveAllChildViews(true);
}

// static
views::Widget* TranslateBubbleView::ShowBubble(
    views::View* anchor_view,
    views::Button* highlighted_button,
    const gfx::Point& anchor_point,
    content::WebContents* web_contents,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    DisplayReason reason) {
  if (translate_bubble_view_) {
    // When the user reads the advanced setting panel, the bubble should not be
    // changed because they are focusing on the bubble.
    if (translate_bubble_view_->web_contents() == web_contents &&
        translate_bubble_view_->model()->GetViewState() ==
            TranslateBubbleModel::VIEW_STATE_ADVANCED) {
      return nullptr;
    }
    if (step != translate::TRANSLATE_STEP_TRANSLATE_ERROR) {
      TranslateBubbleModel::ViewState state =
          TranslateBubbleModelImpl::TranslateStepToViewState(step);
      translate_bubble_view_->SwitchView(state);
    } else {
      translate_bubble_view_->SwitchToErrorView(error_type);
    }
    return nullptr;
  } else {
    if (step == translate::TRANSLATE_STEP_AFTER_TRANSLATE &&
        reason == AUTOMATIC) {
      return nullptr;
    }
  }

  std::string source_language;
  std::string target_language;
  ChromeTranslateClient::GetTranslateLanguages(web_contents, &source_language,
                                               &target_language);

  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
      new translate::TranslateUIDelegate(
          ChromeTranslateClient::GetManagerFromWebContents(web_contents)
              ->GetWeakPtr(),
          source_language, target_language));
  std::unique_ptr<TranslateBubbleModel> model(
      new TranslateBubbleModelImpl(step, std::move(ui_delegate)));
  TranslateBubbleView* view = new TranslateBubbleView(
      anchor_view, anchor_point, std::move(model), error_type, web_contents);

  if (highlighted_button)
    view->SetHighlightedButton(highlighted_button);

  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(view);
  view->ShowForReason(reason);
  translate::ReportUiAction(translate::BUBBLE_SHOWN);
  return bubble_widget;
}

// static
void TranslateBubbleView::CloseCurrentBubble() {
  if (translate_bubble_view_)
    translate_bubble_view_->CloseBubble();
}

// static
TranslateBubbleView* TranslateBubbleView::GetCurrentBubble() {
  return translate_bubble_view_;
}

void TranslateBubbleView::CloseBubble() {
  mouse_handler_.reset();
  LocationBarBubbleDelegateView::CloseBubble();
}

int TranslateBubbleView::GetDialogButtons() const {
  // TODO(estade): this should be using GetDialogButtons().
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 TranslateBubbleView::GetWindowTitle() const {
  int id = 0;
  switch (model_->GetViewState()) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE:
      id = IDS_TRANSLATE_BUBBLE_BEFORE_TRANSLATE_TITLE;
      break;
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
      id = IDS_TRANSLATE_BUBBLE_TRANSLATING;
      break;
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE:
      id = IDS_TRANSLATE_BUBBLE_TRANSLATED_TITLE;
      break;
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      id = IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE;
      break;
    case TranslateBubbleModel::VIEW_STATE_ADVANCED:
      id = IDS_TRANSLATE_BUBBLE_ADVANCED_TITLE;
      break;
  }

  return l10n_util::GetStringUTF16(id);
}

void TranslateBubbleView::Init() {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  should_always_translate_ = model_->ShouldAlwaysTranslate();
  before_translate_view_ = CreateViewBeforeTranslate();
  translating_view_ = CreateViewTranslating();
  after_translate_view_ = CreateViewAfterTranslate();
  error_view_ = CreateViewError();
  advanced_view_ = CreateViewAdvanced();

  AddChildView(before_translate_view_);
  AddChildView(translating_view_);
  AddChildView(after_translate_view_);
  AddChildView(error_view_);
  AddChildView(advanced_view_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));

  UpdateChildVisibilities();

  if (model_->GetViewState() == TranslateBubbleModel::VIEW_STATE_ERROR)
    model_->ShowError(error_type_);
}

void TranslateBubbleView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  switch (static_cast<ButtonID>(sender->id())) {
    case BUTTON_ID_TRANSLATE: {
      Translate();
      break;
    }
    case BUTTON_ID_DONE: {
      ConfirmAdvancedOptions();
      break;
    }
    case BUTTON_ID_CANCEL: {
      model_->GoBackFromAdvanced();
      UpdateChildVisibilities();
      SizeToContents();
      translate::ReportUiAction(translate::CANCEL_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_TRY_AGAIN: {
      model_->Translate();
      translate::ReportUiAction(translate::TRY_AGAIN_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_SHOW_ORIGINAL: {
      ShowOriginal();
      break;
    }
    case BUTTON_ID_ALWAYS_TRANSLATE: {
      views::Checkbox* always_checkbox = GetAlwaysTranslateCheckbox();
      DCHECK(always_checkbox);
      should_always_translate_ = always_checkbox->checked();
      translate::ReportUiAction(should_always_translate_
                                    ? translate::ALWAYS_TRANSLATE_CHECKED
                                    : translate::ALWAYS_TRANSLATE_UNCHECKED);
      break;
    }
    case BUTTON_ID_ADVANCED: {
      SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
      translate::ReportUiAction(translate::ADVANCED_BUTTON_CLICKED);
      break;
    }
    case BUTTON_ID_OPTIONS_MENU: {
      ShowOptionsMenu(sender);
      break;
    }
  }
}

views::View* TranslateBubbleView::GetInitiallyFocusedView() {
  return GetCurrentView()->GetNextFocusableView();
}

bool TranslateBubbleView::ShouldShowCloseButton() const {
  return true;
}

void TranslateBubbleView::WindowClosing() {
  // The operations for |model_| are valid only when a WebContents is alive.
  // TODO(hajimehoshi): TranslateBubbleViewModel(Impl) should not hold a
  // WebContents as a member variable because the WebContents might be destroyed
  // while the TranslateBubbleViewModel(Impl) is still alive. Instead,
  // TranslateBubbleViewModel should take a reference of a WebContents at each
  // method. (crbug/320497)
  if (web_contents())
    model_->OnBubbleClosing();

  // We have to reset |translate_bubble_view_| here, not in our destructor,
  // because we'll be destroyed asynchronously and the shown state will be
  // checked before then.
  DCHECK_EQ(translate_bubble_view_, this);
  translate_bubble_view_ = NULL;
}

bool TranslateBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  switch (model_->GetViewState()) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        Translate();
        return true;
      }
      break;
    }
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
      break;
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        ShowOriginal();
        return true;
      }
      break;
    }
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      break;
    case TranslateBubbleModel::VIEW_STATE_ADVANCED: {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        ConfirmAdvancedOptions();
        return true;
      }
      break;
    }
  }
  return BubbleDialogDelegateView::AcceleratorPressed(accelerator);
}

gfx::Size TranslateBubbleView::CalculatePreferredSize() const {
  int width = 0;
  for (int i = 0; i < child_count(); i++) {
    const views::View* child = child_at(i);
    width = std::max(width, child->GetPreferredSize().width());
  }
  int height = GetCurrentView()->GetPreferredSize().height();
  return gfx::Size(width, height);
}

void TranslateBubbleView::OnPerformAction(views::Combobox* combobox) {
  HandleComboboxPerformAction(static_cast<ComboboxID>(combobox->id()));
}

void TranslateBubbleView::LinkClicked(views::Link* source, int event_flags) {
  HandleLinkClicked(static_cast<LinkID>(source->id()));
}

void TranslateBubbleView::ShowOptionsMenu(views::Button* source) {
  // Recreate the menu model as translated languages can change while the menu
  // is not showing, which invalidates these text strings.
  options_menu_model_.reset(new ui::SimpleMenuModel(this));

  // Don't show "Always translate <language>" in incognito mode, because it
  // doesn't do anything anyways.
  if (!is_in_incognito_window_) {
    options_menu_model_->AddCheckItem(
        OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE,
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
            model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())));
  }

  options_menu_model_->AddItem(
      OptionsMenuItem::NEVER_TRANSLATE_LANGUAGE,
      l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
          model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())));

  if (model_->CanBlacklistSite()) {
    options_menu_model_->AddItemWithStringId(
        OptionsMenuItem::NEVER_TRANSLATE_SITE,
        IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE);
  }

  options_menu_model_->AddItemWithStringId(
      OptionsMenuItem::MORE_OPTIONS,
      IDS_TRANSLATE_BUBBLE_ADVANCED_MENU_BUTTON);

  options_menu_runner_.reset(new views::MenuRunner(
      options_menu_model_.get(), views::MenuRunner::COMBOBOX));
  gfx::Rect screen_bounds = source->GetBoundsInScreen();
  options_menu_runner_->RunMenuAt(source->GetWidget(), nullptr, screen_bounds,
                                  views::MENU_ANCHOR_TOPRIGHT,
                                  ui::MENU_SOURCE_MOUSE);
}

bool TranslateBubbleView::IsCommandIdChecked(int command_id) const {
  DCHECK_EQ(OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE, command_id);
  return should_always_translate_;
}

bool TranslateBubbleView::IsCommandIdEnabled(int command_id) const {
  return true;
}

void TranslateBubbleView::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case OptionsMenuItem::ALWAYS_TRANSLATE_LANGUAGE:
      should_always_translate_ = !should_always_translate_;
      model_->SetAlwaysTranslate(should_always_translate_);

      if (should_always_translate_ &&
          model_->GetViewState() ==
              TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) {
        model_->Translate();
        SwitchView(TranslateBubbleModel::VIEW_STATE_TRANSLATING);
      }
      break;

    case OptionsMenuItem::NEVER_TRANSLATE_LANGUAGE:
      translate::ReportUiAction(
          translate::NEVER_TRANSLATE_LANGUAGE_MENU_CLICKED);
      model_->SetNeverTranslateLanguage(true);
      model_->DeclineTranslation();
      GetWidget()->Close();
      break;
    case OptionsMenuItem::NEVER_TRANSLATE_SITE:
      translate::ReportUiAction(translate::NEVER_TRANSLATE_SITE_MENU_CLICKED);
      model_->SetNeverTranslateSite(true);
      model_->DeclineTranslation();
      GetWidget()->Close();
      break;
    case OptionsMenuItem::MORE_OPTIONS:
      translate::ReportUiAction(translate::ADVANCED_MENU_CLICKED);
      SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
      break;
    default:
      NOTREACHED();
  }
}

void TranslateBubbleView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
  translate::ReportUiAction(translate::ADVANCED_LINK_CLICKED);
}

void TranslateBubbleView::OnWidgetClosing(views::Widget* widget) {
  if (GetBubbleFrameView()->close_button_clicked()) {
    model_->DeclineTranslation();
    translate::ReportUiAction(translate::CLOSE_BUTTON_CLICKED);
  }
}

TranslateBubbleModel::ViewState TranslateBubbleView::GetViewState() const {
  return model_->GetViewState();
}

TranslateBubbleView::TranslateBubbleView(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    std::unique_ptr<TranslateBubbleModel> model,
    translate::TranslateErrors::Type error_type,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor_view, anchor_point, web_contents),
      before_translate_view_(NULL),
      translating_view_(NULL),
      after_translate_view_(NULL),
      error_view_(NULL),
      advanced_view_(NULL),
      source_language_combobox_(NULL),
      target_language_combobox_(NULL),
      before_always_translate_checkbox_(NULL),
      advanced_always_translate_checkbox_(NULL),
      advanced_cancel_button_(NULL),
      advanced_done_button_(NULL),
      before_translate_options_button_(NULL),
      model_(std::move(model)),
      error_type_(error_type),
      is_in_incognito_window_(
          web_contents && web_contents->GetBrowserContext()->IsOffTheRecord()),
      should_always_translate_(false) {
  translate_bubble_view_ = this;
  if (web_contents)  // web_contents can be null in unit_tests.
    mouse_handler_.reset(new WebContentMouseHandler(this, web_contents));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::TRANSLATE);
}

views::View* TranslateBubbleView::GetCurrentView() const {
  switch (model_->GetViewState()) {
    case TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE:
      return before_translate_view_;
    case TranslateBubbleModel::VIEW_STATE_TRANSLATING:
      return translating_view_;
    case TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE:
      return after_translate_view_;
    case TranslateBubbleModel::VIEW_STATE_ERROR:
      return error_view_;
    case TranslateBubbleModel::VIEW_STATE_ADVANCED:
      return advanced_view_;
  }
  NOTREACHED();
  return NULL;
}

void TranslateBubbleView::Translate() {
  model_->SetAlwaysTranslate(should_always_translate_);
  model_->Translate();
  translate::ReportUiAction(translate::TRANSLATE_BUTTON_CLICKED);
}

void TranslateBubbleView::ShowOriginal() {
  model_->RevertTranslation();
  GetWidget()->Close();
  translate::ReportUiAction(translate::SHOW_ORIGINAL_BUTTON_CLICKED);
}

void TranslateBubbleView::ConfirmAdvancedOptions() {
  model_->SetAlwaysTranslate(should_always_translate_);
  if (model_->IsPageTranslatedInCurrentLanguages()) {
    model_->GoBackFromAdvanced();
    UpdateChildVisibilities();
    SizeToContents();
  } else {
    model_->Translate();
    SwitchView(TranslateBubbleModel::VIEW_STATE_TRANSLATING);
  }
  translate::ReportUiAction(translate::DONE_BUTTON_CLICKED);
}

void TranslateBubbleView::HandleLinkClicked(
    TranslateBubbleView::LinkID sender_id) {
  switch (sender_id) {
    case LINK_ID_ADVANCED: {
      SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);
      translate::ReportUiAction(translate::ADVANCED_LINK_CLICKED);
      break;
    }
  }
}

void TranslateBubbleView::HandleComboboxPerformAction(
    TranslateBubbleView::ComboboxID sender_id) {
  switch (sender_id) {
    case COMBOBOX_ID_SOURCE_LANGUAGE: {
      if (model_->GetOriginalLanguageIndex() ==
          source_language_combobox_->selected_index()) {
        break;
      }
      model_->UpdateOriginalLanguageIndex(
          source_language_combobox_->selected_index());
      UpdateAdvancedView();
      translate::ReportUiAction(translate::SOURCE_LANGUAGE_MENU_CLICKED);
      break;
    }
    case COMBOBOX_ID_TARGET_LANGUAGE: {
      if (model_->GetTargetLanguageIndex() ==
          target_language_combobox_->selected_index()) {
        break;
      }
      model_->UpdateTargetLanguageIndex(
          target_language_combobox_->selected_index());
      UpdateAdvancedView();
      translate::ReportUiAction(translate::TARGET_LANGUAGE_MENU_CLICKED);
      break;
    }
  }
}

void TranslateBubbleView::UpdateChildVisibilities() {
  // Update the state of the always translate checkbox
  if (advanced_always_translate_checkbox_)
    advanced_always_translate_checkbox_->SetChecked(should_always_translate_);
  if (before_always_translate_checkbox_)
    before_always_translate_checkbox_->SetChecked(should_always_translate_);
  for (int i = 0; i < child_count(); i++) {
    views::View* view = child_at(i);
    view->SetVisible(view == GetCurrentView());
  }
  if (GetWidget())
    GetWidget()->UpdateWindowTitle();
  // BoxLayout only considers visible children, so ensure any newly visible
  // child views are positioned correctly.
  Layout();
}

views::View* TranslateBubbleView::CreateViewBeforeTranslate() {
  base::string16 original_language_name =
      model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex());
  if (original_language_name.empty()) {
    original_language_name =
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_UNKNOWN_LANGUAGE);
  }

  views::View* view = new views::View();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  constexpr int kCheckboxColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kCheckboxColumnSetId);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);

  constexpr int kButtonColumnSetId = 1;
  cs = layout->AddColumnSet(kButtonColumnSetId);
  cs->AddPaddingColumn(1.0, 0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);

  if (model_->ShouldShowAlwaysTranslateShortcut()) {
    layout->StartRow(views::GridLayout::kFixedSize, kCheckboxColumnSetId);
    before_always_translate_checkbox_ = new views::Checkbox(
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_BUBBLE_ALWAYS_TRANSLATE_LANG,
            model_->GetLanguageNameAt(model_->GetOriginalLanguageIndex())),
        this);
    before_always_translate_checkbox_->set_id(BUTTON_ID_ALWAYS_TRANSLATE);
    layout->AddView(before_always_translate_checkbox_);
  }

  layout->StartRowWithPadding(
      views::GridLayout::kFixedSize, kButtonColumnSetId,
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  views::LabelButton* accept_button =
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ACCEPT));
  accept_button->set_id(BUTTON_ID_TRANSLATE);

  accept_button->SetIsDefault(true);
  before_translate_options_button_ =
      views::MdTextButton::CreateSecondaryUiButton(
          this,
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  before_translate_options_button_->set_id(BUTTON_ID_OPTIONS_MENU);
  before_translate_options_button_->set_request_focus_on_press(true);

  if (views::PlatformStyle::kIsOkButtonLeading) {
    layout->AddView(accept_button);
    layout->AddView(before_translate_options_button_);
  } else {
    layout->AddView(before_translate_options_button_);
    layout->AddView(accept_button);
  }

  return view;
}

views::View* TranslateBubbleView::CreateViewTranslating() {
  base::string16 target_language_name =
      model_->GetLanguageNameAt(model_->GetTargetLanguageIndex());

  views::View* view = new views::View();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddPaddingColumn(1.0, 0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                views::GridLayout::kFixedSize, 0);

  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                views::GridLayout::kFixedSize, 0);

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  views::LabelButton* revert_button =
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT));
  revert_button->set_id(BUTTON_ID_SHOW_ORIGINAL);
  revert_button->SetEnabled(false);
  layout->AddView(revert_button);

  return view;
}

views::View* TranslateBubbleView::CreateViewAfterTranslate() {
  views::View* view = new views::View();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddPaddingColumn(1.0, 0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                views::GridLayout::kFixedSize, 0);

  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_REVERT));
  button->set_id(BUTTON_ID_SHOW_ORIGINAL);
  layout->AddView(button);

  views::Button* options_menu_button =
      views::MdTextButton::CreateSecondaryUiButton(
          this,
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_OPTIONS_MENU_BUTTON));
  options_menu_button->set_id(BUTTON_ID_OPTIONS_MENU);
  options_menu_button->set_request_focus_on_press(true);

  layout->AddView(options_menu_button);

  return view;
}

views::View* TranslateBubbleView::CreateViewError() {
  views::View* view = new views::View();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddPaddingColumn(1.0, 0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);

  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  views::LabelButton* try_again_button =
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRY_AGAIN));
  try_again_button->set_id(BUTTON_ID_TRY_AGAIN);
  layout->AddView(try_again_button);

  views::LabelButton* advanced_button =
      views::MdTextButton::CreateSecondaryUiButton(
          this,
          l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ADVANCED_BUTTON));
  advanced_button->set_id(BUTTON_ID_ADVANCED);
  layout->AddView(advanced_button);

  return view;
}

// TODO(hajimehoshi): Revice this later to show a specific message for each
// error. (crbug/307350)
views::View* TranslateBubbleView::CreateViewAdvanced() {
  views::Label* source_language_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_PAGE_LANGUAGE));

  views::Label* target_language_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_TRANSLATION_LANGUAGE));

  int source_default_index = model_->GetOriginalLanguageIndex();
  source_language_combobox_model_.reset(
      new LanguageComboboxModel(source_default_index, model_.get()));
  source_language_combobox_ =
      new views::Combobox(source_language_combobox_model_.get());

  source_language_combobox_->set_id(COMBOBOX_ID_SOURCE_LANGUAGE);
  source_language_combobox_->set_listener(this);

  int target_default_index = model_->GetTargetLanguageIndex();
  target_language_combobox_model_.reset(
      new LanguageComboboxModel(target_default_index, model_.get()));
  target_language_combobox_ =
      new views::Combobox(target_language_combobox_model_.get());

  target_language_combobox_->set_id(COMBOBOX_ID_TARGET_LANGUAGE);
  target_language_combobox_->set_listener(this);

  // In an incognito window, "Always translate" checkbox shouldn't be shown.
  if (!is_in_incognito_window_) {
    advanced_always_translate_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_ALWAYS), this);
    advanced_always_translate_checkbox_->set_id(BUTTON_ID_ALWAYS_TRANSLATE);
  }

  views::View* view = new AdvancedViewContainer();
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  enum {
    COLUMN_SET_ID_LANGUAGES,
    COLUMN_SET_ID_BUTTONS,
    COLUMN_SET_ID_ALWAYS_CHECKBOX,
  };

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_LANGUAGES);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);
  cs->AddPaddingColumn(1.0, 0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_BUTTONS);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);
  cs->AddPaddingColumn(
      1.0, provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);
  cs->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);

  cs = layout->AddColumnSet(COLUMN_SET_ID_ALWAYS_CHECKBOX);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0,
                0);

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(source_language_label);
  layout->AddView(source_language_combobox_);

  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_LANGUAGES);
  layout->AddView(target_language_label);
  layout->AddView(target_language_combobox_);

  if (!is_in_incognito_window_) {
    layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);
    layout->StartRow(views::GridLayout::kFixedSize,
                     COLUMN_SET_ID_ALWAYS_CHECKBOX);
    layout->AddView(advanced_always_translate_checkbox_);
  }

  layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);

  layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_BUTTONS);
  layout->SkipColumns(1);

  advanced_done_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_DONE));
  advanced_done_button_->set_id(BUTTON_ID_DONE);
  advanced_done_button_->SetIsDefault(true);
  advanced_cancel_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_CANCEL));
  advanced_cancel_button_->set_id(BUTTON_ID_CANCEL);
  layout->AddView(advanced_done_button_);
  layout->AddView(advanced_cancel_button_);

  UpdateAdvancedView();

  return view;
}

views::Checkbox* TranslateBubbleView::GetAlwaysTranslateCheckbox() {
  if (model_->GetViewState() == TranslateBubbleModel::VIEW_STATE_ADVANCED) {
    return advanced_always_translate_checkbox_;
  } else if (model_->GetViewState() ==
             TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) {
    return before_always_translate_checkbox_;
  } else {
    NOTREACHED();
    return nullptr;
  }
}

void TranslateBubbleView::SwitchView(
    TranslateBubbleModel::ViewState view_state) {
  if (model_->GetViewState() == view_state)
    return;

  model_->SetViewState(view_state);
  if (view_state == TranslateBubbleModel::VIEW_STATE_ADVANCED)
    UpdateAdvancedView();
  UpdateChildVisibilities();
  SizeToContents();
}

void TranslateBubbleView::SwitchToErrorView(
    translate::TranslateErrors::Type error_type) {
  SwitchView(TranslateBubbleModel::VIEW_STATE_ERROR);
  error_type_ = error_type;
  model_->ShowError(error_type);
}

void TranslateBubbleView::UpdateAdvancedView() {
  DCHECK(advanced_done_button_);
  advanced_done_button_->SetText(
      l10n_util::GetStringUTF16(model_->IsPageTranslatedInCurrentLanguages()
                                    ? IDS_DONE
                                    : IDS_TRANSLATE_BUBBLE_ACCEPT));
}
