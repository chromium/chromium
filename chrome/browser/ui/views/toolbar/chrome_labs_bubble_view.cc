// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "base/bind.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace {

ChromeLabsBubbleView* g_chrome_labs_bubble = nullptr;

}  // namespace

// TODO(elainechien): Add screenshots and strings for translation when UI is
// finished.
class ChromeLabsFooter : public views::View {
 public:
  ChromeLabsFooter() {
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical);
    AddChildView(views::Builder<views::Label>()
                     .CopyAddressTo(&restart_label_)
                     .SetText(base::ASCIIToUTF16(
                         "Your changes will take effect the next time you "
                         "relaunch Google Chrome."))
                     .SetMultiLine(true)
                     .Build());
    AddChildView(views::Builder<views::MdTextButton>()
                     .CopyAddressTo(&restart_button_)
                     .SetCallback(base::BindRepeating(&chrome::AttemptRestart))
                     .SetText(base::ASCIIToUTF16("Relaunch"))
                     .Build());
    restart_label_->SizeToFit(
        restart_button_->CalculatePreferredSize().width());
  }
  // views::View
  gfx::Size CalculatePreferredSize() const override {
    int width = restart_button_->CalculatePreferredSize().width();
    int height = GetHeightForWidth(width);
    return gfx::Size(width, height);
  }

 private:
  views::MdTextButton* restart_button_;
  views::Label* restart_label_;
};

// static
void ChromeLabsBubbleView::Show(views::View* anchor_view) {
  g_chrome_labs_bubble = new ChromeLabsBubbleView(
      anchor_view, std::make_unique<ChromeLabsBubbleViewModel>());
  views::Widget* const widget =
      BubbleDialogDelegateView::CreateBubble(g_chrome_labs_bubble);
  widget->Show();
}

ChromeLabsBubbleView::ChromeLabsBubbleView(
    views::View* anchor_view,
    std::unique_ptr<ChromeLabsBubbleViewModel> model)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      model_(std::move(model)) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  SetTitle(base::ASCIIToUTF16("Chrome Labs"));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetDefault(views::kMarginsKey, gfx::Insets(5));

  // TODO(elainechien): ChromeOS specific logic for creating FlagsStorage
  flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
      g_browser_process->local_state());
  flags_state_ = about_flags::GetCurrentFlagsState();

  menu_item_container_ = AddChildView(std::make_unique<views::View>());
  menu_item_container_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kMarginsKey, gfx::Insets(10));

  // Create each lab item.
  std::vector<std::string> lab_internal_names = model_->GetLabInfo();
  for (const auto& internal_name : lab_internal_names) {
    const flags_ui::FeatureEntry* entry =
        flags_state_->FindFeatureEntryByName(internal_name);
    DCHECK_EQ(entry->type, flags_ui::FeatureEntry::FEATURE_VALUE);
    int default_index = GetIndexOfEnabledLabState(entry);
    menu_item_container_->AddChildView(
        CreateLabItem(internal_name, default_index, entry));
  }
  // TODO(elainechien): Build UI for 0 experiments case.
  DCHECK(menu_item_container_->children().size() >= 1);

  restart_prompt_ = AddChildView(std::make_unique<ChromeLabsFooter>());
  restart_prompt_->SetVisible(about_flags::IsRestartNeededToCommitChanges());
}

std::unique_ptr<ChromeLabsItemView> ChromeLabsBubbleView::CreateLabItem(
    std::string internal_name,
    int default_index,
    const flags_ui::FeatureEntry* entry) {
  auto combobox_callback = [](ChromeLabsBubbleView* bubble_view,
                              std::string internal_name,
                              ChromeLabsItemView* item_view) {
    int selected_index = item_view->GetSelectedIndex();
    about_flags::SetFeatureEntryEnabled(
        bubble_view->flags_storage_.get(),
        internal_name + "@" + base::NumberToString(selected_index), true);
    bubble_view->ShowRelaunchPrompt();
  };

  return std::make_unique<ChromeLabsItemView>(
      internal_name, default_index, entry,
      base::BindRepeating(combobox_callback, this, internal_name));
}

int ChromeLabsBubbleView::GetIndexOfEnabledLabState(
    const flags_ui::FeatureEntry* entry) {
  std::set<std::string> enabled_entries;
  flags_state_->GetSanitizedEnabledFlags(flags_storage_.get(),
                                         &enabled_entries);
  for (int i = 0; i < entry->NumOptions(); i++) {
    const std::string name = entry->NameForOption(i);
    if (enabled_entries.count(name) > 0)
      return i;
  }
  return 0;
}

void ChromeLabsBubbleView::ShowRelaunchPrompt() {
  restart_prompt_->SetVisible(about_flags::IsRestartNeededToCommitChanges());
  DCHECK_EQ(g_chrome_labs_bubble, this);
  g_chrome_labs_bubble->SizeToContents();
}

ChromeLabsBubbleView::~ChromeLabsBubbleView() {
  g_chrome_labs_bubble = nullptr;
}

// static
bool ChromeLabsBubbleView::IsShowing() {
  return g_chrome_labs_bubble != nullptr &&
         g_chrome_labs_bubble->GetWidget() != nullptr;
}

// static
void ChromeLabsBubbleView::Hide() {
  if (IsShowing())
    g_chrome_labs_bubble->GetWidget()->Close();
}

// static
ChromeLabsBubbleView*
ChromeLabsBubbleView::GetChromeLabsBubbleViewForTesting() {
  return g_chrome_labs_bubble;
}

flags_ui::FlagsState* ChromeLabsBubbleView::GetFlagsStateForTesting() {
  return flags_state_;
}

flags_ui::FlagsStorage* ChromeLabsBubbleView::GetFlagsStorageForTesting() {
  return flags_storage_.get();
}

views::View* ChromeLabsBubbleView::GetMenuItemContainerForTesting() {
  return menu_item_container_;
}

bool ChromeLabsBubbleView::IsRestartPromptVisibleForTesting() {
  return restart_prompt_->GetVisible();
}
