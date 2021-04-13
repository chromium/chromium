// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/version_info/channel.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ChromeLabsSelectedLab {
  kUnspecifiedSelected = 0,
  kReadLaterSelected = 1,
  kTabScrollingSelected = 3,
  kMaxValue = kTabScrollingSelected,
};

void EmitToHistogram(const std::u16string& selected_lab_state,
                     const std::string& internal_name) {
  const auto get_histogram_name = [](const std::u16string& selected_lab_state) {
    if (selected_lab_state == base::ASCIIToUTF16(base::StringPiece(
                                  flags_ui::kGenericExperimentChoiceDefault))) {
      return "Toolbar.ChromeLabs.DefaultLabAction";
    } else if (selected_lab_state ==
               base::ASCIIToUTF16(base::StringPiece(
                   flags_ui::kGenericExperimentChoiceEnabled))) {
      return "Toolbar.ChromeLabs.EnableLabAction";
    } else if (selected_lab_state ==
               base::ASCIIToUTF16(base::StringPiece(
                   flags_ui::kGenericExperimentChoiceDisabled))) {
      return "Toolbar.ChromeLabs.DisableLabAction";
    } else {
      return "";
    }
  };

  const auto get_enum = [](const std::string& internal_name) {
    if (internal_name == flag_descriptions::kReadLaterFlagId) {
      return ChromeLabsSelectedLab::kReadLaterSelected;
    } else if (internal_name == flag_descriptions::kScrollableTabStripFlagId) {
      return ChromeLabsSelectedLab::kTabScrollingSelected;
    } else {
      return ChromeLabsSelectedLab::kUnspecifiedSelected;
    }
  };

  const std::string histogram_name = get_histogram_name(selected_lab_state);
  if (!histogram_name.empty())
    base::UmaHistogramEnumeration(histogram_name, get_enum(internal_name));
}

ChromeLabsBubbleView* g_chrome_labs_bubble = nullptr;

class ChromeLabsFooter : public views::View {
 public:
  METADATA_HEADER(ChromeLabsFooter);
  ChromeLabsFooter() {
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    AddChildView(
        views::Builder<views::Label>()
            .CopyAddressTo(&restart_label_)
            .SetText(l10n_util::GetStringUTF16(
                IDS_CHROMELABS_RELAUNCH_FOOTER_MESSAGE))
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .SetProperty(views::kFlexBehaviorKey,
                         views::FlexSpecification(
                             views::MinimumFlexSizeRule::kPreferred,
                             views::MaximumFlexSizeRule::kPreferred, true))
            .SetBorder(views::CreateEmptyBorder(
                gfx::Insets(0, 0,
                            views::LayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_CONTROL_VERTICAL),
                            0)))
            .Build());
    AddChildView(views::Builder<views::MdTextButton>()
                     .CopyAddressTo(&restart_button_)
                     .SetCallback(base::BindRepeating(&chrome::AttemptRestart))
                     .SetText(l10n_util::GetStringUTF16(
                         IDS_CHROMELABS_RELAUNCH_BUTTON_LABEL))
                     .SetProminent(true)
                     .Build());
    SetBackground(views::CreateThemedSolidBackground(
        this, ui::NativeTheme::kColorId_BubbleFooterBackground));
    SetBorder(views::CreateEmptyBorder(
        views::LayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG)));
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred, true));
  }
 private:
  views::MdTextButton* restart_button_;
  views::Label* restart_label_;
};

BEGIN_METADATA(ChromeLabsFooter, views::View)
END_METADATA

}  // namespace

// static
void ChromeLabsBubbleView::Show(views::View* anchor_view,
                                Browser* browser,
                                const ChromeLabsBubbleViewModel* model) {
  g_chrome_labs_bubble = new ChromeLabsBubbleView(anchor_view, browser, model);
  views::Widget* const widget =
      BubbleDialogDelegateView::CreateBubble(g_chrome_labs_bubble);
  widget->Show();
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

ChromeLabsBubbleView::~ChromeLabsBubbleView() {
  g_chrome_labs_bubble = nullptr;
}

ChromeLabsBubbleView::ChromeLabsBubbleView(
    views::View* anchor_view,
    Browser* browser,
    const ChromeLabsBubbleViewModel* model)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      model_(model) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  SetTitle(l10n_util::GetStringUTF16(IDS_WINDOW_TITLE_EXPERIMENTS));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(gfx::Insets(0));
  SetEnableArrowKeyTraversal(true);

  // TODO(elainechien): ChromeOS specific logic for creating FlagsStorage
  flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
      g_browser_process->local_state());
  flags_state_ = about_flags::GetCurrentFlagsState();

  menu_item_container_ = AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kScaleToZero,
                           views::MaximumFlexSizeRule::kPreferred, true))
          .SetBorder(views::CreateEmptyBorder(
              views::LayoutProvider::Get()->GetInsetsMetric(
                  views::INSETS_DIALOG)))
          .Build());

  // Create each lab item.
  const std::vector<LabInfo>& all_labs = model_->GetLabInfo();
  for (const auto& lab : all_labs) {
    const flags_ui::FeatureEntry* entry =
        flags_state_->FindFeatureEntryByName(lab.internal_name);
    if (IsFeatureSupportedOnChannel(lab) &&
        IsFeatureSupportedOnPlatform(entry) &&
        !about_flags::ShouldSkipConditionalFeatureEntry(flags_storage_.get(),
                                                        *entry)) {
      bool valid_entry_type =
          entry->type == flags_ui::FeatureEntry::FEATURE_VALUE ||
          entry->type == flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE;
      DCHECK(valid_entry_type);
      int default_index = GetIndexOfEnabledLabState(entry);
      menu_item_container_->AddChildView(
          CreateLabItem(lab, default_index, entry, browser));
    }
  }
  // ChromeLabsButton should not appear in the toolbar if there are no
  // experiments to show. Therefore ChromeLabsBubble should not be created.
  DCHECK(menu_item_container_->children().size() >= 1);

  restart_prompt_ = AddChildView(std::make_unique<ChromeLabsFooter>());
  restart_prompt_->SetVisible(about_flags::IsRestartNeededToCommitChanges());
}

std::unique_ptr<ChromeLabsItemView> ChromeLabsBubbleView::CreateLabItem(
    const LabInfo& lab,
    int default_index,
    const flags_ui::FeatureEntry* entry,
    Browser* browser) {
  auto combobox_callback = [](ChromeLabsBubbleView* bubble_view,
                              std::string internal_name,
                              ChromeLabsItemView* item_view) {
    int selected_index = item_view->GetSelectedIndex();
    about_flags::SetFeatureEntryEnabled(
        bubble_view->flags_storage_.get(),
        internal_name + flags_ui::kMultiSeparatorChar +
            base::NumberToString(selected_index),
        true);
    bubble_view->ShowRelaunchPrompt();
    EmitToHistogram(
        item_view->GetFeatureEntry()->DescriptionForOption(selected_index),
        internal_name);
  };

  std::unique_ptr<ChromeLabsItemView> item_view =
      std::make_unique<ChromeLabsItemView>(
          lab, default_index, entry,
          base::BindRepeating(combobox_callback, this, lab.internal_name),
          browser);

  item_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred, true));
  return item_view;
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

bool ChromeLabsBubbleView::IsFeatureSupportedOnChannel(const LabInfo& lab) {
  return chrome::GetChannel() <= lab.allowed_channel;
}

// TODO(elainechien): ChromeOS specific logic for owner access only flags.
bool ChromeLabsBubbleView::IsFeatureSupportedOnPlatform(
    const flags_ui::FeatureEntry* entry) {
  return (entry && (entry->supported_platforms &
                    flags_ui::FlagsState::GetCurrentPlatform()) != 0);
}

void ChromeLabsBubbleView::ShowRelaunchPrompt() {
  restart_prompt_->SetVisible(about_flags::IsRestartNeededToCommitChanges());

  // Manually announce the relaunch footer message because VoiceOver doesn't
  // announces the message when the footer appears.
#if defined(OS_MAC)
  if (restart_prompt_->GetVisible()) {
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_CHROMELABS_RELAUNCH_FOOTER_MESSAGE));
  }
#endif

  DCHECK_EQ(g_chrome_labs_bubble, this);
  g_chrome_labs_bubble->SizeToContents();
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

BEGIN_METADATA(ChromeLabsBubbleView, views::BubbleDialogDelegateView)
END_METADATA
