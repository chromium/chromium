// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_combobox_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"

namespace {

const char kGlobalSidePanelRegistryKey[] = "global_side_panel_registry_key";

constexpr int kSidePanelContentViewId = 42;
constexpr int kSidePanelContentWrapperViewId = 43;

constexpr SidePanelEntry::Id kDefaultEntry = SidePanelEntry::Id::kReadingList;

std::unique_ptr<views::ImageButton> CreateControlButton(
    views::View* host,
    base::RepeatingClosure pressed_callback,
    const gfx::VectorIcon& icon,
    const gfx::Insets& margin_insets,
    const std::u16string& tooltip_text,
    int dip_size) {
  auto button = views::CreateVectorImageButtonWithNativeTheme(pressed_callback,
                                                              icon, dip_size);
  button->SetTooltipText(tooltip_text);
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetProperty(views::kMarginsKey, margin_insets);
  views::InstallCircleHighlightPathGenerator(button.get());
  button->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_left(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));

  return button;
}

class SidePanelSeparator : public views::Separator {
 public:
  METADATA_HEADER(SidePanelSeparator);

  void OnThemeChanged() override {
    views::Separator::OnThemeChanged();
    SetColor(GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_SIDE_PANEL_CONTENT_AREA_SEPARATOR));
  }
};

BEGIN_METADATA(SidePanelSeparator, views::Separator)
END_METADATA

using PopulateSidePanelCallback = base::OnceCallback<void(
    SidePanelEntry* entry,
    absl::optional<std::unique_ptr<views::View>> content_view)>;

// SidePanelContentSwappingContainer is used as the content wrapper for views
// hosted in the side panel. This uses the SidePanelContentProxy to check if or
// wait for a SidePanelEntry's content view to be ready to be shown then only
// swaps the views when the content is ready. If the SidePanelContextProxy
// doesn't exist, the content is swapped immediately.
class SidePanelContentSwappingContainer : public views::View {
 public:
  explicit SidePanelContentSwappingContainer(bool show_immediately_for_testing)
      : show_immediately_for_testing_(show_immediately_for_testing) {
    SetUseDefaultFillLayout(true);
    SetBackground(
        views::CreateThemedSolidBackground(kColorSidePanelBackground));
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    SetID(kSidePanelContentWrapperViewId);
  }

  ~SidePanelContentSwappingContainer() override {
    ResetLoadingEntryIfNecessary();
  }

  void RequestEntry(SidePanelEntry* entry, PopulateSidePanelCallback callback) {
    DCHECK(entry);
    ResetLoadingEntryIfNecessary();
    auto content_view = entry->GetContent();
    SidePanelContentProxy* content_proxy =
        SidePanelUtil::GetSidePanelContentProxy(content_view.get());
    if (content_proxy->IsAvailable() || show_immediately_for_testing_) {
      std::move(callback).Run(entry, std::move(content_view));
    } else {
      entry->CacheView(std::move(content_view));
      loading_entry_ = entry;
      loaded_callback_ = std::move(callback);
      content_proxy->SetAvailableCallback(
          base::BindOnce(&SidePanelContentSwappingContainer::RunLoadedCallback,
                         base::Unretained(this)));
    }
  }

 private:
  void RunLoadedCallback() {
    DCHECK(!loaded_callback_.is_null());
    SidePanelEntry* entry = loading_entry_;
    loading_entry_ = nullptr;
    std::move(loaded_callback_).Run(entry, absl::nullopt);
  }

  void ResetLoadingEntryIfNecessary() {
    if (loading_entry_ && loading_entry_->CachedView()) {
      SidePanelUtil::GetSidePanelContentProxy(loading_entry_->CachedView())
          ->ResetAvailableCallback();
    }
    loading_entry_ = nullptr;
  }

  // When true, don't delay switching panels.
  bool show_immediately_for_testing_;
  // If the SidePanelEntry is ever discarded by the SidePanelCoordinator then we
  // are always either immediately switching to a different entry (where this
  // value would be reset) or closing the side panel (where this would be
  // destroyed).
  raw_ptr<SidePanelEntry> loading_entry_ = nullptr;
  PopulateSidePanelCallback loaded_callback_;
};

}  // namespace

SidePanelCoordinator::SidePanelCoordinator(BrowserView* browser_view)
    : browser_view_(browser_view) {
  combobox_model_ = std::make_unique<SidePanelComboboxModel>();

  auto global_registry = std::make_unique<SidePanelRegistry>();
  global_registry->AddObserver(this);
  global_registry_ = global_registry.get();
  browser_view->browser()->SetUserData(kGlobalSidePanelRegistryKey,
                                       std::move(global_registry));

  browser_view_->browser()->tab_strip_model()->AddObserver(this);

  SidePanelUtil::PopulateGlobalEntries(browser_view->browser(),
                                       GetGlobalSidePanelRegistry());
}

SidePanelCoordinator::~SidePanelCoordinator() {
  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);
}

void SidePanelCoordinator::Show(absl::optional<SidePanelEntry::Id> entry_id) {
  if (!entry_id.has_value())
    entry_id = GetLastActiveEntryId().value_or(kDefaultEntry);

  SidePanelEntry* entry = GetEntryForId(entry_id.value());
  if (!entry)
    return;

  if (GetContentView() == nullptr) {
    InitializeSidePanel();
    base::RecordAction(base::UserMetricsAction("SidePanel.Show"));
    // Record usage for side panel promo.
    feature_engagement::TrackerFactory::GetForBrowserContext(
        browser_view_->GetProfile())
        ->NotifyEvent("side_panel_shown");

    // Close IPH for side panel if shown.
    browser_view_->browser()->window()->CloseFeaturePromo(
        feature_engagement::kIPHReadingListInSidePanelFeature);
  }

  // Ensure that the correct combobox entry is selected. This may not be the
  // case if `Show()` was called after registering a contextual entry.
  if (entry_id.has_value()) {
    DCHECK(header_combobox_);
    header_combobox_->SetSelectedIndex(
        combobox_model_->GetIndexForId(entry_id.value()));
  }

  SidePanelContentSwappingContainer* content_wrapper =
      static_cast<SidePanelContentSwappingContainer*>(
          GetContentView()->GetViewByID(kSidePanelContentWrapperViewId));
  DCHECK(content_wrapper);
  content_wrapper->RequestEntry(
      entry, base::BindOnce(&SidePanelCoordinator::PopulateSidePanel,
                            base::Unretained(this)));
}

void SidePanelCoordinator::Close() {
  if (!GetContentView())
    return;

  if (current_entry_) {
    // Reset current_entry_ first to prevent current_entry->OnEntryHidden() from
    // calling multiple times. This could happen in the edge cases when callback
    // inside current_entry->OnEntryHidden() is calling Close() to trigger race
    // condition.
    auto* current_entry = current_entry_.get();
    current_entry_.reset();
    current_entry->OnEntryHidden();
  }

  if (global_registry_->active_entry().has_value()) {
    last_active_global_entry_id_ =
        global_registry_->active_entry().value()->id();
  }
  // Reset active entry values for all observed registries and clear cache for
  // everything except remaining active entries (i.e. if another tab has an
  // active contextual entry).
  global_registry_->ResetActiveEntry();
  if (auto* contextual_registry = GetActiveContextualRegistry())
    contextual_registry->ResetActiveEntry();
  ClearCachedEntryViews();

  // TODO(pbos): Make this button observe panel-visibility state instead.
  browser_view_->toolbar()->side_panel_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));

  // `OnEntryWillDeregister` (triggered by calling `OnEntryHidden`) may already
  // have deleted the content view, so check that it still exists.
  if (views::View* content_view = GetContentView())
    browser_view_->right_aligned_side_panel()->RemoveChildViewT(content_view);
  header_combobox_ = nullptr;
  base::RecordAction(base::UserMetricsAction("SidePanel.Hide"));
}

void SidePanelCoordinator::Toggle() {
  if (GetContentView() != nullptr) {
    Close();
  } else {
    Show();
  }
}

SidePanelRegistry* SidePanelCoordinator::GetGlobalSidePanelRegistry() {
  return static_cast<SidePanelRegistry*>(
      browser_view_->browser()->GetUserData(kGlobalSidePanelRegistryKey));
}

void SidePanelCoordinator::SetNoDelaysForTesting() {
  no_delays_for_testing_ = true;
}

views::View* SidePanelCoordinator::GetContentView() {
  return browser_view_->right_aligned_side_panel()->GetViewByID(
      kSidePanelContentViewId);
}

SidePanelEntry* SidePanelCoordinator::GetEntryForId(
    SidePanelEntry::Id entry_id) {
  if (auto* entry = global_registry_->GetEntryForId(entry_id))
    return entry;
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    if (auto* entry = contextual_registry->GetEntryForId(entry_id))
      return entry;
  }
  return nullptr;
}

void SidePanelCoordinator::InitializeSidePanel() {
  // TODO(pbos): Make this button observe panel-visibility state instead.
  browser_view_->toolbar()->side_panel_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_HIDE));

  auto container = std::make_unique<views::FlexLayoutView>();
  // Align views vertically top to bottom.
  container->SetOrientation(views::LayoutOrientation::kVertical);
  container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  // Stretch views to fill horizontal bounds.
  container->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  container->SetID(kSidePanelContentViewId);

  container->AddChildView(CreateHeader());
  container->AddChildView(std::make_unique<SidePanelSeparator>());

  auto content_wrapper = std::make_unique<SidePanelContentSwappingContainer>(
      no_delays_for_testing_);
  container->AddChildView(std::move(content_wrapper));
  // Set to not visible so that the side panel is not shown until content is
  // ready to be shown.
  container->SetVisible(false);

  browser_view_->right_aligned_side_panel()->AddChildView(std::move(container));
}

void SidePanelCoordinator::PopulateSidePanel(
    SidePanelEntry* entry,
    absl::optional<std::unique_ptr<views::View>> content_view) {
  if (current_entry_.get() == entry) {
    return;
  }

  auto* content_wrapper =
      GetContentView()->GetViewByID(kSidePanelContentWrapperViewId);
  DCHECK(content_wrapper);
  // |content_wrapper| should have either no child views or one child view for
  // the currently hosted SidePanelEntry.
  DCHECK(content_wrapper->children().size() <= 1);

  GetContentView()->SetVisible(true);
  if (current_entry_ && content_wrapper->children().size()) {
    auto current_entry_view =
        content_wrapper->RemoveChildViewT(content_wrapper->children().front());
    current_entry_->CacheView(std::move(current_entry_view));
    current_entry_->OnEntryHidden();
  }
  content_wrapper->AddChildView(content_view.has_value()
                                    ? std::move(content_view.value())
                                    : entry->GetContent());
  if (auto* contextual_registry = GetActiveContextualRegistry())
    contextual_registry->ResetActiveEntry();
  current_entry_ = entry->GetWeakPtr();
  entry->OnEntryShown();
}

void SidePanelCoordinator::ClearCachedEntryViews() {
  global_registry_->ClearCachedEntryViews();
  TabStripModel* model = browser_view_->browser()->tab_strip_model();
  if (!model)
    return;
  for (int index = 0; index < model->count(); ++index) {
    auto* web_contents =
        browser_view_->browser()->tab_strip_model()->GetWebContentsAt(index);
    if (auto* registry = SidePanelRegistry::Get(web_contents))
      registry->ClearCachedEntryViews();
  }
}

absl::optional<SidePanelEntry::Id> SidePanelCoordinator::GetLastActiveEntryId()
    const {
  // If a contextual entry is active, return that. If not, return the last
  // active global entry. If neither exist, fall back to kReadingList.
  if (GetActiveContextualRegistry() &&
      GetActiveContextualRegistry()->active_entry().has_value()) {
    return GetActiveContextualRegistry()->active_entry().value()->id();
  }

  if (global_registry_->active_entry().has_value())
    return global_registry_->active_entry().value()->id();

  if (last_active_global_entry_id_.has_value())
    return last_active_global_entry_id_.value();

  return absl::nullopt;
}

absl::optional<SidePanelEntry::Id> SidePanelCoordinator::GetSelectedId() const {
  if (!header_combobox_)
    return absl::nullopt;
  return combobox_model_->GetIdAt(header_combobox_->GetSelectedIndex());
}

SidePanelRegistry* SidePanelCoordinator::GetActiveContextualRegistry() const {
  if (auto* web_contents =
          browser_view_->browser()->tab_strip_model()->GetActiveWebContents()) {
    return SidePanelRegistry::Get(web_contents);
  }
  return nullptr;
}

std::unique_ptr<views::View> SidePanelCoordinator::CreateHeader() {
  auto header = std::make_unique<views::FlexLayoutView>();
  // ChromeLayoutProvider for providing margins.
  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();

  // Set the interior margins of the header on the left and right sides.
  header->SetInteriorMargin(gfx::Insets::VH(
      0, chrome_layout_provider->GetDistanceMetric(
             views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  // Set alignments for horizontal (main) and vertical (cross) axes.
  header->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The minimum cross axis size should the expected height of the header.
  constexpr int kDefaultSidePanelHeaderHeight = 40;
  header->SetMinimumCrossAxisSize(kDefaultSidePanelHeaderHeight);
  header->SetBackground(
      views::CreateThemedSolidBackground(ui::kColorWindowBackground));

  header_combobox_ = header->AddChildView(CreateCombobox());

  header->AddChildView(CreateControlButton(
      header.get(),
      base::BindRepeating(&SidePanelCoordinator::Close, base::Unretained(this)),
      views::kIcCloseIcon, gfx::Insets(),
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));

  return header;
}

std::unique_ptr<views::Combobox> SidePanelCoordinator::CreateCombobox() {
  auto combobox = std::make_unique<views::Combobox>(combobox_model_.get());
  combobox->SetSelectedIndex(combobox_model_->GetIndexForId(
      GetLastActiveEntryId().value_or(kDefaultEntry)));
  // TODO(corising): Replace this with something appropriate.
  combobox->SetAccessibleName(
      combobox_model_->GetItemAt(combobox->GetSelectedIndex()));

  combobox->SetCallback(base::BindRepeating(
      &SidePanelCoordinator::OnComboboxChanged, base::Unretained(this)));
  combobox->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/false)
          .WithAlignment(views::LayoutAlignment::kStart));
  return combobox;
}

void SidePanelCoordinator::OnComboboxChanged() {
  SidePanelEntry::Id entry_id =
      combobox_model_->GetIdAt(header_combobox_->GetSelectedIndex());
  Show(entry_id);
}

void SidePanelCoordinator::OnEntryRegistered(SidePanelEntry* entry) {
  combobox_model_->AddItem(entry);
}

void SidePanelCoordinator::OnEntryWillDeregister(SidePanelEntry* entry) {
  absl::optional<SidePanelEntry::Id> selected_id = GetSelectedId();
  combobox_model_->RemoveItem(entry->id());

  // Update the current entry to make sure we don't show an entry that is being
  // removed or close the panel if the entry being deregistered is the only one
  // that has been visible.
  if (GetContentView() && selected_id.has_value() &&
      selected_id.value() == entry->id()) {
    if (global_registry_->active_entry().has_value()) {
      Show(GetLastActiveEntryId().value_or(kDefaultEntry));
    } else {
      Close();
    }
  }
}

void SidePanelCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  // Handle removing the previous tab's contextual registry if one exists and
  // update the combobox.
  if (auto* old_contextual_registry =
          SidePanelRegistry::Get(selection.old_contents)) {
    old_contextual_registry->RemoveObserver(this);
    combobox_model_->RemoveItems(old_contextual_registry->entries());
  }

  // Add the current tab's contextual registry and update the combobox.
  auto* new_contextual_registry =
      SidePanelRegistry::Get(selection.new_contents);
  if (new_contextual_registry) {
    new_contextual_registry->AddObserver(this);
    combobox_model_->AddItems(new_contextual_registry->entries());
  }

  // If an active entry is available, show it. If not, close the panel.
  if (GetContentView()) {
    if ((!new_contextual_registry ||
         !new_contextual_registry->active_entry().has_value()) &&
        !global_registry_->active_entry().has_value()) {
      Close();
    } else {
      Show(GetLastActiveEntryId().value_or(kDefaultEntry));
    }
  } else if (new_contextual_registry &&
             new_contextual_registry->active_entry().has_value()) {
    Show(new_contextual_registry->active_entry().value()->id());
  }
}
