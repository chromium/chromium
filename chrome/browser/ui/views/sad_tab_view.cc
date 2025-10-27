// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sad_tab_view.h"

#include <algorithm>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/bulleted_label_list/bulleted_label_list_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

std::unique_ptr<views::Label> CreateFormattedLabel(
    const std::u16string& message) {
  auto label = std::make_unique<views::Label>(
      message, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetLineHeight(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  return label;
}

// Show the error code in a selectable label to allow users to copy it.
std::unique_ptr<views::Label> CreateErrorCodeLabel(int format_string,
                                                   int error_code) {
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          format_string,
          base::UTF8ToUTF16(content::CrashExitCodeToString(error_code))),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetSelectable(true);
  label->SetFontList(
      gfx::FontList().Derive(-3, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
  return label;
}

}  // namespace

SadTabView::SadTabView(content::WebContents* web_contents, SadTabKind kind)
    : SadTab(web_contents, kind) {
  // This view gets inserted as a child of a WebView, but we don't want the
  // WebView to delete us if the WebView gets deleted before the SadTabHelper
  // does.
  set_owned_by_client(OwnedByClientPassKey());

  SetBackground(views::CreateSolidBackground(ui::kColorDialogBackground));
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::INSETS_DIALOG)));
  auto* top_spacer = AddChildView(std::make_unique<views::View>());
  auto* container = AddChildView(std::make_unique<views::FlexLayoutView>());
  container->SetOrientation(views::LayoutOrientation::kVertical);
  constexpr int kMinContentWidth = 240;
  container->SetMinimumCrossAxisSize(kMinContentWidth);
  auto* bottom_spacer = AddChildView(std::make_unique<views::View>());

  // Center content horizontally; divide vertical padding into 1/3 above, 2/3
  // below.
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->SetFlexForView(top_spacer, 1);
  layout->SetFlexForView(bottom_spacer, 2);

  // Crashed tab image.
  auto* image = container->AddChildView(std::make_unique<views::ImageView>());
  image->SetImage(
      ui::ImageModel::FromVectorIcon(kCrashedTabIcon, ui::kColorIcon, 48));
  const int unrelated_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  image->SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(0, 0, unrelated_vertical_spacing, 0));
  image->SetProperty(views::kCrossAxisAlignmentKey,
                     views::LayoutAlignment::kStart);

  // Title.
  title_ = container->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(GetTitle())));
  title_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::LargeFont));
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  constexpr int kTitleBottomSpacing = 13;
  title_->SetProperty(views::kMarginsKey,
                      gfx::Insets::TLBR(0, 0, kTitleBottomSpacing, 0));

  // Message and optional bulleted list.
  message_ = container->AddChildView(
      CreateFormattedLabel(l10n_util::GetStringUTF16(GetInfoMessage())));
  std::vector<int> bullet_string_ids = GetSubMessages();
  if (!bullet_string_ids.empty()) {
    std::vector<std::u16string> texts;
    std::ranges::transform(bullet_string_ids, std::back_inserter(texts),
                           l10n_util::GetStringUTF16);
    auto* list_view =
        container->AddChildView(std::make_unique<views::BulletedLabelListView>(
            std::move(texts), views::style::TextStyle::STYLE_PRIMARY));
    list_view->SetProperty(views::kTableColAndRowSpanKey, gfx::Size(2, 1));
  }

  // Error code.
  container
      ->AddChildView(CreateErrorCodeLabel(GetErrorCodeFormatString(),
                                          GetCrashedErrorCode()))
      ->SetProperty(views::kMarginsKey,
                    gfx::Insets::TLBR(kTitleBottomSpacing, 0,
                                      unrelated_vertical_spacing, 0));

  // Bottom row: help link, action button.
  auto* actions_container =
      container->AddChildView(std::make_unique<views::FlexLayoutView>());
  actions_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // TODO(crbug.com/363826230): See View::SetLayoutManagerUseConstrainedSpace.
  //
  // `actions_container` is a horizontal FlexLayout, and its child element
  // `action_button` has an unbounded horizontal size. This causes it to consume
  // the size of the entire constraint space when we calculate the preferred
  // size under the current constraint space. This causes the actual width
  // occupied by action_button to be too wide.
  //
  // There is currently no good way to handle kEnd alignment for a single
  // element.
  actions_container->SetLayoutManagerUseConstrainedSpace(false);

  EnableHelpLink(actions_container);

  action_button_ =
      actions_container->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&SadTabView::PerformAction,
                              base::Unretained(this), Action::kButton),
          l10n_util::GetStringUTF16(GetButtonTitle())));
  action_button_->SetStyle(ui::ButtonStyle::kProminent);
  action_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithAlignment(views::LayoutAlignment::kEnd));

  // Needed to ensure this View is drawn even if a sibling (such as dev tools)
  // has a z-order.
  SetPaintToLayer();
  AttachToWebView();

  if (owner_) {
    // If the `owner_` ContentsWebView has a rounded background, the sad tab
    // should also have matching rounded corners as well.
    SetBackgroundRadii(
        static_cast<ContentsWebView*>(owner_)->GetBackgroundRadii());
  }

  // Make the accessibility role of this view an alert dialog, and
  // put focus on the action button. This causes screen readers to
  // immediately announce the text of this view.
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  if (action_button_->GetWidget() && action_button_->GetWidget()->IsActive()) {
    action_button_->RequestFocus();
  }
}

SadTabView::~SadTabView() {
  if (owner_) {
    owner_->SetCrashedOverlayView(nullptr);
  }
}

void SadTabView::ReinstallInWebView() {
  if (owner_) {
    owner_->SetCrashedOverlayView(nullptr);
    owner_ = nullptr;
  }
  AttachToWebView();
}

gfx::RoundedCornersF SadTabView::GetBackgroundRadii() const {
  CHECK(layer());
  return layer()->rounded_corner_radii();
}

void SadTabView::SetBackgroundRadii(const gfx::RoundedCornersF& radii) {
  // Since SadTabView paints onto its own layer and it is leaf layer, we can
  // round the background by applying rounded corners to the layer without
  // clipping any other browser content.
  CHECK(layer());
  layer()->SetRoundedCornerRadius(radii);
  layer()->SetIsFastRoundedCorner(/*enable=*/true);
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

void SadTabView::AttachToWebView() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  // This can be null during prefetch.
  if (!browser) {
    return;
  }

  // In unit tests, browser->window() might not be a real BrowserView.
  if (!browser->window()->GetNativeWindow()) {
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  DCHECK(browser_view);

  std::vector<ContentsWebView*> visible_contents_views =
      browser_view->GetAllVisibleContentsWebViews();
  for (ContentsWebView* contents_view : visible_contents_views) {
    if (contents_view->web_contents() == web_contents()) {
      owner_ = contents_view;
      owner_->SetCrashedOverlayView(this);
      break;
    }
  }
}

void SadTabView::EnableHelpLink(views::FlexLayoutView* actions_container) {
#if BUILDFLAG(IS_CHROMEOS)
  // Do not show the help link in the kiosk session to prevent escape from a
  // kiosk app.
  if (chromeos::IsKioskSession()) {
    return;
  }
#endif
  auto* help_link =
      actions_container->AddChildView(std::make_unique<views::Link>(
          l10n_util::GetStringUTF16(GetHelpLinkTitle())));
  help_link->SetCallback(base::BindRepeating(
      &SadTab::PerformAction, base::Unretained(this), Action::kHelpLink));
  help_link->SetProperty(views::kTableVertAlignKey,
                         views::LayoutAlignment::kCenter);
}

void SadTabView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Specify the maximum message and title width explicitly.
  constexpr int kMaxContentWidth = 600;
  const int max_width =
      std::min(width() - ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL) *
                             2,
               kMaxContentWidth);

  message_->SizeToFit(max_width);
  title_->SizeToFit(max_width);
}

std::unique_ptr<SadTab> SadTab::Create(content::WebContents* web_contents,
                                       SadTabKind kind) {
  return std::make_unique<SadTabView>(web_contents, kind);
}

BEGIN_METADATA(SadTabView)
END_METADATA
