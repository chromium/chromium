// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include <memory>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_later_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_combobox_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/read_later/side_panel/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/read_later/side_panel/reader_mode/reader_mode_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
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
constexpr int kSidePanelContentViewId = 42;
constexpr int kSidePanelContentWrapperViewId = 43;

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

  return button;
}
}  // namespace

SidePanelCoordinator::SidePanelCoordinator(BrowserView* browser_view,
                                           SidePanelRegistry* global_registry)
    : browser_view_(browser_view), global_registry_(global_registry) {
  combobox_model_ = std::make_unique<SidePanelComboboxModel>();
  global_registry->AddObserver(this);
  // TODO(pbos): Consider moving creation of SidePanelEntry into other functions
  // that can easily be unit tested.
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadingList,
      l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE),
      ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
      base::BindRepeating(
          [](SidePanelCoordinator* coordinator,
             Browser* browser) -> std::unique_ptr<views::View> {
            return std::make_unique<ReadLaterSidePanelWebView>(
                browser, base::BindRepeating(&SidePanelCoordinator::Close,
                                             base::Unretained(coordinator)));
          },
          this, browser_view->browser())));
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kBookmarks,
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE),
      ui::ImageModel::FromVectorIcon(omnibox::kStarIcon, ui::kColorIcon),
      base::BindRepeating(&SidePanelCoordinator::CreateBookmarksWebView,
                          base::Unretained(this), browser_view->browser())));
  if (features::IsReaderModeSidePanelEnabled()) {
    global_registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kReaderMode,
        l10n_util::GetStringUTF16(IDS_READER_MODE_TITLE),
        ui::ImageModel::FromVectorIcon(kReaderModeIcon, ui::kColorIcon),
        base::BindRepeating(&SidePanelCoordinator::CreateReaderModeWebView,
                            base::Unretained(this), browser_view->browser())));
  }
}

SidePanelCoordinator::~SidePanelCoordinator() = default;

void SidePanelCoordinator::Show(absl::optional<SidePanelEntry::Id> entry_id) {
  if (!entry_id.has_value()) {
    // TODO(corising): Handle choosing between last active entries when there
    // are multiple registries.
    entry_id = GetLastActiveEntry();
  }

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

  PopulateSidePanel(entry);
}

void SidePanelCoordinator::Close() {
  views::View* const content_view = GetContentView();
  if (!content_view)
    return;

  // TODO(pbos): Make this button observe panel-visibility state instead.
  browser_view_->toolbar()->side_panel_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));

  browser_view_->right_aligned_side_panel()->RemoveChildViewT(content_view);
  base::RecordAction(base::UserMetricsAction("SidePanel.Hide"));
}

void SidePanelCoordinator::Toggle() {
  if (GetContentView() != nullptr) {
    Close();
  } else {
    Show();
  }
}

views::View* SidePanelCoordinator::GetContentView() {
  return browser_view_->right_aligned_side_panel()->GetViewByID(
      kSidePanelContentViewId);
}

SidePanelEntry* SidePanelCoordinator::GetEntryForId(
    SidePanelEntry::Id entry_id) {
  for (auto const& entry : global_registry_->entries()) {
    if (entry.get()->id() == entry_id)
      return entry.get();
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
  auto* separator =
      container->AddChildView(std::make_unique<views::Separator>());
  // TODO(pbos): Make sure this separator updates per theme changes and does not
  // pull color provider from BrowserView directly. This is wrong (wrong
  // provider, wrong to call this before we know it's added to widget and wrong
  // not to update as the theme changes).
  const ui::ThemeProvider* const theme_provider =
      browser_view_->GetThemeProvider();
  // TODO(pbos): Stop inlining this color (de-duplicate this, SidePanelBorder
  // and BrowserView).
  separator->SetColor(color_utils::GetResultingPaintColor(
      theme_provider->GetColor(
          ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR),
      theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR)));

  auto content_wrapper = std::make_unique<views::View>();
  content_wrapper->SetUseDefaultFillLayout(true);
  content_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  content_wrapper->SetID(kSidePanelContentWrapperViewId);
  container->AddChildView(std::move(content_wrapper));

  browser_view_->right_aligned_side_panel()->AddChildView(std::move(container));
}

void SidePanelCoordinator::PopulateSidePanel(SidePanelEntry* entry) {
  views::View* content_wrapper =
      GetContentView()->GetViewByID(kSidePanelContentWrapperViewId);
  DCHECK(content_wrapper);
  content_wrapper->RemoveAllChildViews();
  content_wrapper->AddChildView(entry->CreateContent());
  entry->OnEntryShown();
}

SidePanelEntry::Id SidePanelCoordinator::GetLastActiveEntry() const {
  return global_registry_->last_active_entry().has_value()
             ? global_registry_->last_active_entry().value()
             : SidePanelEntry::Id::kReadingList;
}

std::unique_ptr<views::View> SidePanelCoordinator::CreateHeader() {
  auto header = std::make_unique<views::FlexLayoutView>();
  // ChromeLayoutProvider for providing margins.
  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();

  // Set the interior margins of the header on the left and right sides.
  header->SetInteriorMargin(gfx::Insets(
      0, chrome_layout_provider->GetDistanceMetric(
             views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  // Set alignments for horizontal (main) and vertical (cross) axes.
  header->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The minimum cross axis size should the expected height of the header.
  constexpr int kDefaultSidePanelHeaderHeight = 40;
  header->SetMinimumCrossAxisSize(kDefaultSidePanelHeaderHeight);
  header->SetBackground(views::CreateThemedSolidBackground(
      header.get(), ui::kColorWindowBackground));

  header_combobox_ = header->AddChildView(CreateCombobox());

  // Create an empty view between branding and buttons to align branding on left
  // without hardcoding margins. This view fills up the empty space between the
  // branding and the control buttons.
  // TODO(pbos): This View seems like it should be avoidable by not having LHS
  // and RHS content stretch? This is copied from the Lens side panel, but could
  // probably by cleaned up?
  auto container = std::make_unique<views::View>();
  container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  header->AddChildView(std::move(container));

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
  combobox->SetSelectedIndex(
      combobox_model_->GetIndexForId(GetLastActiveEntry()));
  // TODO(corising): Replace this with something appropriate.
  combobox->SetAccessibleName(
      combobox_model_->GetItemAt(combobox->GetSelectedIndex()));

  combobox->SetCallback(base::BindRepeating(
      &SidePanelCoordinator::OnComboboxChanged, base::Unretained(this)));
  return combobox;
}

void SidePanelCoordinator::OnComboboxChanged() {
  SidePanelEntry::Id entry_id =
      combobox_model_->GetIdAt(header_combobox_->GetSelectedIndex());
  Show(entry_id);
}

std::unique_ptr<views::View> SidePanelCoordinator::CreateBookmarksWebView(
    Browser* browser) {
  auto bookmarks_web_view =
      std::make_unique<SidePanelWebUIViewT<BookmarksSidePanelUI>>(
          browser, base::RepeatingClosure(),
          base::BindRepeating(&SidePanelCoordinator::Close,
                              base::Unretained(this)),
          std::make_unique<BubbleContentsWrapperT<BookmarksSidePanelUI>>(
              GURL(chrome::kChromeUIBookmarksSidePanelURL), browser->profile(),
              IDS_BOOKMARK_MANAGER_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false));
  if (base::FeatureList::IsEnabled(features::kSidePanelDragAndDrop)) {
    extensions::BookmarkManagerPrivateDragEventRouter::CreateForWebContents(
        bookmarks_web_view.get()->contents_wrapper()->web_contents());
  }
  return bookmarks_web_view;
}

std::unique_ptr<views::View> SidePanelCoordinator::CreateReaderModeWebView(
    Browser* browser) {
  return std::make_unique<SidePanelWebUIViewT<ReaderModeSidePanelUI>>(
      browser, base::RepeatingClosure(),
      base::BindRepeating(&SidePanelCoordinator::Close, base::Unretained(this)),
      std::make_unique<BubbleContentsWrapperT<ReaderModeSidePanelUI>>(
          GURL(chrome::kChromeUIReaderModeSidePanelURL), browser->profile(),
          IDS_READER_MODE_TITLE,
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false));
}

void SidePanelCoordinator::OnEntryRegistered(SidePanelEntry* entry) {
  combobox_model_->AddItem(entry);
}
