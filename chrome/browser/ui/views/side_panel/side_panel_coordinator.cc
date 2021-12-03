// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include <memory>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_later_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"

namespace {
constexpr int kSidePanelContentViewId = 42;

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

SidePanelCoordinator::SidePanelCoordinator(BrowserView* browser_view)
    : browser_view_(browser_view) {
  // TODO(pbos): Consider moving creation of SidePanelEntry into other functions
  // that can easily be unit tested.
  window_registry_.Register(std::make_unique<SidePanelEntry>(
      l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE),
      base::BindRepeating(
          [](SidePanelCoordinator* coordinator,
             Browser* browser) -> std::unique_ptr<views::View> {
            return std::make_unique<ReadLaterSidePanelWebView>(
                browser, base::BindRepeating(&SidePanelCoordinator::Close,
                                             base::Unretained(coordinator)));
          },
          this, browser_view->browser())));
}

SidePanelCoordinator::~SidePanelCoordinator() = default;

void SidePanelCoordinator::Show() {
  if (GetContentView() != nullptr)
    return;
  // TODO(pbos): Make this button observe panel-visibility state instead.
  browser_view_->toolbar()->side_panel_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_HIDE));

  // TODO(pbos): Handle multiple entries.
  DCHECK_EQ(1u, window_registry_.entries().size());
  SidePanelEntry* const entry = window_registry_.entries().front().get();

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

  // TODO(pbos): Set some ID on this container so that we can replace the
  // content in here from the combobox once it exists.
  auto content_wrapper = std::make_unique<views::View>();
  content_wrapper->SetUseDefaultFillLayout(true);
  content_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  content_wrapper->AddChildView(entry->CreateContent());

  container->AddChildView(std::move(content_wrapper));

  browser_view_->right_aligned_side_panel()->AddChildView(std::move(container));
}

void SidePanelCoordinator::Close() {
  views::View* const content_view = GetContentView();
  if (!content_view)
    return;

  // TODO(pbos): Make this button observe panel-visibility state instead.
  browser_view_->toolbar()->side_panel_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));

  browser_view_->right_aligned_side_panel()->RemoveChildViewT(content_view);
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

  // TODO(pbos): Replace this with a combobox. Note that this combobox will need
  // to listen to changes to the window registry. This combobox should probably
  // call SidePanelCoordinator::ShowPanel(panel_id) or similar. This method or
  // ID does not exist, `panel_id` would probably need to be added to
  // SidePanelEntry unless we want to use raw pointers. This also implies that
  // SidePanelCoordinator needs a link to where the SidePanelEntry content is
  // showing so that it can be replaced (perhaps via a view ID for
  // `content_wrapper` above).
  DCHECK_EQ(1u, window_registry_.entries().size());
  SidePanelEntry* const entry = window_registry_.entries().front().get();
  header->AddChildView(std::make_unique<views::Label>(entry->name()));

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
