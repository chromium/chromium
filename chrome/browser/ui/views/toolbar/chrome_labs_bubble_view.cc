// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_utils.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/about_flags.h"
#endif

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ChromeLabsSelectedLab {
  kUnspecifiedSelected = 0,
  // kReadLaterSelected = 1,
  // kTabSearchSelected = 2,
  kTabScrollingSelected = 3,
  kSidePanelSelected = 4,
  kLensRegionSearchSelected = 5,
  kWebUITabStripSelected = 6,
  kMaxValue = kWebUITabStripSelected,
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
    if (internal_name == flag_descriptions::kScrollableTabStripFlagId) {
      return ChromeLabsSelectedLab::kTabScrollingSelected;
    } else if (internal_name == flag_descriptions::kSidePanelFlagId) {
      return ChromeLabsSelectedLab::kSidePanelSelected;
    } else if (internal_name ==
               flag_descriptions::kEnableLensRegionSearchFlagId) {
      return ChromeLabsSelectedLab::kLensRegionSearchSelected;
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && \
    (defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH))
    } else if (internal_name == flag_descriptions::kWebUITabStripFlagId) {
      return ChromeLabsSelectedLab::kWebUITabStripSelected;
#endif
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
  explicit ChromeLabsFooter(ChromeLabsBubbleView* bubble) {
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
                     .SetCallback(base::BindRepeating(
                         [](ChromeLabsBubbleView* bubble_view) {
                           bubble_view->RestartToApplyFlags();
                         },
                         bubble))
                     .SetText(l10n_util::GetStringUTF16(
                         IDS_CHROMELABS_RELAUNCH_BUTTON_LABEL))
                     .SetProminent(true)
                     .Build());
    SetBackground(views::CreateThemedSolidBackground(
        this, ui::kColorBubbleFooterBackground));
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
void ChromeLabsBubbleView::Show(ChromeLabsButton* anchor_view,
                                Browser* browser,
                                const ChromeLabsBubbleViewModel* model,
                                bool user_is_chromeos_owner) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (anchor_view->GetAshOwnerCheckTimer()) {
    UmaHistogramMediumTimes("Toolbar.ChromeLabs.AshOwnerCheckTime",
                            anchor_view->GetAshOwnerCheckTimer()->Elapsed());
  }
#endif
  g_chrome_labs_bubble = new ChromeLabsBubbleView(anchor_view, browser, model,
                                                  user_is_chromeos_owner);
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
    ChromeLabsButton* anchor_view,
    Browser* browser,
    const ChromeLabsBubbleViewModel* model,
    bool user_is_chromeos_owner)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      model_(model) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  SetTitle(l10n_util::GetStringUTF16(IDS_WINDOW_TITLE_EXPERIMENTS));
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::BoxLayout::Orientation::kVertical);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(gfx::Insets(0));
  SetEnableArrowKeyTraversal(true);
  // Set `kDialog` to avoid the BubbleDialogDelegate returning a default of
  // `kAlertDialog` which would tell screen readers to announce all contents of
  // the bubble when it opens and previous accessibility feedback said that
  // behavior was confusing.
  SetAccessibleRole(ax::mojom::Role::kDialog);

// TODO(elainechien): Take care of additional cases 1) kSafeMode switch is
// present 2) user is secondary user.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_ = browser->profile()->GetOriginalProfile();
  if (user_is_chromeos_owner) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_);
    flags_storage_ = std::make_unique<ash::about_flags::OwnerFlagsStorage>(
        profile_->GetPrefs(), service);
  } else {
    flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        profile_->GetPrefs());
  }
#else
  flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
      g_browser_process->local_state());
#endif
  flags_state_ = about_flags::GetCurrentFlagsState();

  // TODO(crbug.com/1259763): Currently basing this off what extension menu uses
  // for sizing as suggested as an initial fix by UI. Discuss a more formal
  // solution.
  constexpr int kMaxChromeLabsHeightDp = 448;
  auto scroll_view = std::make_unique<views::ScrollView>();
  // TODO(elainechien): Check with UI whether we want to draw overflow
  // indicator.
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  menu_item_container_ = scroll_view->SetContents(
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
  scroll_view->ClipHeightTo(0, kMaxChromeLabsHeightDp);
  AddChildView(std::move(scroll_view));

  // Create each lab item.
  const std::vector<LabInfo>& all_labs = model_->GetLabInfo();
  for (const auto& lab : all_labs) {
    const flags_ui::FeatureEntry* entry =
        flags_state_->FindFeatureEntryByName(lab.internal_name);
    if (IsChromeLabsFeatureValid(lab, browser->profile())) {
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

  // Hide dot indicator once bubble has been opened.
  anchor_view->HideDotIndicator();

  restart_prompt_ = AddChildView(std::make_unique<ChromeLabsFooter>(this));
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

void ChromeLabsBubbleView::RestartToApplyFlags() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS be less intrusive and restart inside the user session after
  // we apply the newly selected flags.
  VLOG(1) << "Restarting to apply per-session flags...";
  ash::about_flags::FeatureFlagsUpdate(*flags_storage_, profile_->GetPrefs())
      .UpdateSessionManager();
#endif
  chrome::AttemptRestart();
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

views::View* ChromeLabsBubbleView::GetMenuItemContainerForTesting() {
  return menu_item_container_;
}

bool ChromeLabsBubbleView::IsRestartPromptVisibleForTesting() {
  return restart_prompt_->GetVisible();
}

BEGIN_METADATA(ChromeLabsBubbleView, views::BubbleDialogDelegateView)
END_METADATA
