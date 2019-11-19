// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sad_tab_view.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/bulleted_label_list_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kMaxContentWidth = 600;
constexpr int kMinColumnWidth = 120;
constexpr int kTitleBottomSpacing = 13;

std::unique_ptr<views::Label> CreateFormattedLabel(
    const base::string16& message) {
  auto label = std::make_unique<views::Label>(
      message, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetLineHeight(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  return label;
}

}  // namespace

SadTabView::SadTabView(content::WebContents* web_contents, SadTabKind kind)
    : SadTab(web_contents, kind) {
  // This view gets inserted as a child of a WebView, but we don't want the
  // WebView to delete us if the WebView gets deleted before the SadTabHelper
  // does.
  set_owned_by_client();

  SetBackground(views::CreateThemedSolidBackground(
      this, ui::NativeTheme::kColorId_DialogBackground));

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  const int column_set_id = 0;
  views::ColumnSet* columns = layout->AddColumnSet(column_set_id);

  // TODO(ananta)
  // This view should probably be styled as web UI.
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int unrelated_horizontal_spacing = provider->GetDistanceMetric(
          DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  columns->AddPaddingColumn(1.0, unrelated_horizontal_spacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, kMinColumnWidth);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, kMinColumnWidth);
  columns->AddPaddingColumn(1.0, unrelated_horizontal_spacing);

  auto image = std::make_unique<views::ImageView>();

  image->SetImage(
      gfx::CreateVectorIcon(kCrashedTabIcon, 48, gfx::kChromeIconGrey));

  const int unrelated_vertical_spacing_large = provider->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE);
  layout->AddPaddingRow(1.0, unrelated_vertical_spacing_large);
  layout->StartRow(views::GridLayout::kFixedSize, column_set_id);
  layout->AddView(std::move(image), 2, 1);

  auto title =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(GetTitle()));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title->SetFontList(rb.GetFontList(ui::ResourceBundle::LargeFont));
  title->SetMultiLine(true);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, column_set_id,
                              views::GridLayout::kFixedSize,
                              unrelated_vertical_spacing_large);
  title_ = layout->AddView(std::move(title), 2, 1.0);

  layout->StartRowWithPadding(views::GridLayout::kFixedSize, column_set_id,
                              views::GridLayout::kFixedSize,
                              kTitleBottomSpacing);
  message_ = layout->AddView(
      CreateFormattedLabel(l10n_util::GetStringUTF16(GetInfoMessage())), 2, 1.0,
      views::GridLayout::LEADING, views::GridLayout::LEADING);

  std::vector<int> bullet_string_ids = GetSubMessages();
  if (!bullet_string_ids.empty()) {
    auto list_view = std::make_unique<BulletedLabelListView>();
    for (const auto& id : bullet_string_ids)
      list_view->AddLabel(l10n_util::GetStringUTF16(id));

    layout->StartRow(views::GridLayout::kFixedSize, column_set_id);
    layout->AddView(std::move(list_view), 2, 1.0);
  }

  std::unique_ptr<views::LabelButton> action_button =
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, l10n_util::GetStringUTF16(GetButtonTitle()));
  auto help_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(GetHelpLinkTitle()));
  help_link->set_listener(this);
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, column_set_id,
                              views::GridLayout::kFixedSize,
                              unrelated_vertical_spacing_large);
  help_link_ =
      layout->AddView(std::move(help_link), 1.0, 1.0,
                      views::GridLayout::LEADING, views::GridLayout::CENTER);
  action_button_ =
      layout->AddView(std::move(action_button), 1.0, 1.0,
                      views::GridLayout::TRAILING, views::GridLayout::LEADING);

  layout->AddPaddingRow(2, provider->GetDistanceMetric(
                               views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  // Needed to ensure this View is drawn even if a sibling (such as dev tools)
  // has a z-order.
  SetPaintToLayer();

  AttachToWebView();

  // Make the accessibility role of this view an alert dialog, and
  // put focus on the action button. This causes screen readers to
  // immediately announce the text of this view.
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kDialog);
  if (action_button_->GetWidget() && action_button_->GetWidget()->IsActive())
    action_button_->RequestFocus();
}

SadTabView::~SadTabView() {
  if (owner_)
    owner_->SetCrashedOverlayView(nullptr);
}

void SadTabView::ReinstallInWebView() {
  if (owner_) {
    owner_->SetCrashedOverlayView(nullptr);
    owner_ = nullptr;
  }
  AttachToWebView();
}

void SadTabView::AttachToWebView() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  // This can be null during prefetch.
  if (!browser)
    return;

  // In unit tests, browser->window() might not be a real BrowserView.
  if (!browser->window()->GetNativeWindow())
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  DCHECK(browser_view);

  views::WebView* web_view = browser_view->contents_web_view();
  if (web_view->GetWebContents() == web_contents()) {
    owner_ = web_view;
    owner_->SetCrashedOverlayView(this);
  }
}

void SadTabView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Specify the maximum message and title width explicitly.
  const int max_width =
      std::min(width() - ChromeLayoutProvider::Get()->GetDistanceMetric(
                             DISTANCE_UNRELATED_CONTROL_HORIZONTAL) *
                             2,
               kMaxContentWidth);

  message_->SizeToFit(max_width);
  title_->SizeToFit(max_width);
}

void SadTabView::LinkClicked(views::Link* source, int event_flags) {
  PerformAction(Action::HELP_LINK);
}

void SadTabView::ButtonPressed(views::Button* sender,
                               const ui::Event& event) {
  DCHECK_EQ(action_button_, sender);
  PerformAction(Action::BUTTON);
}

void SadTabView::OnPaint(gfx::Canvas* canvas) {
  if (!painted_) {
    RecordFirstPaint();
    painted_ = true;
  }
  View::OnPaint(canvas);
}

void SadTabView::RemovedFromWidget() {
  owner_ = nullptr;
}

SadTab* SadTab::Create(content::WebContents* web_contents,
                       SadTabKind kind) {
  return new SadTabView(web_contents, kind);
}

BEGIN_METADATA(SadTabView)
METADATA_PARENT_CLASS(views::View)
END_METADATA()
