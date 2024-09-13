// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/enterprise/managed_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"

namespace {
constexpr int kMenuIconSize = 16;
constexpr int kWindowIconSize = 24;
constexpr int kContentWidth = 280;
constexpr int kContentGap = 12;
constexpr int kContentMargin = 16;

void AddDisclaimerSection(views::View* parent,
                          const ui::ImageModel& icon,
                          const std::u16string& text,
                          int bottom_margin = 0) {
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(0, 0, bottom_margin, 0));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* browsing_data_management_section =
      parent->AddChildView(views::Builder<views::View>()
                               .SetLayoutManager(std::move(box_layout))
                               .Build());

  browsing_data_management_section->AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(icon)
          .SetBorder(
              views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 0, kContentGap)))
          .Build());

  browsing_data_management_section->AddChildView(
      views::Builder<views::Label>()
          .SetText(text)
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetTextStyle(views::style::STYLE_BODY_4)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 0)))
          .SetMultiLine(true)
          .Build());
}

}  //  namespace

ManagedMenuView::ManagedMenuView(views::Button* anchor_button, Browser* browser)
    : views::BubbleDialogDelegateView(anchor_button,
                                      views::BubbleBorder::TOP_RIGHT),
      browser_(browser) {
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetDefaultButton(ui::DIALOG_BUTTON_NONE);
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(GetAccessibleWindowTitle());
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  SetButtonStyle(ui::DIALOG_BUTTON_OK, ui::ButtonStyle::kTonal);
  SetAcceptCallback(base::BindOnce(&ManagedMenuView::OpenManagementPage,
                                   weak_factory_.GetWeakPtr()));
  SetShowCloseButton(true);

  profile_pref_change_registrar_.Init(GetProfile()->GetPrefs());
  profile_pref_change_registrar_.Add(
      prefs::kEnterpriseCustomLabel,
      base::BindRepeating(&ManagedMenuView::RebuildView,
                          weak_factory_.GetWeakPtr()));
  profile_pref_change_registrar_.Add(
      prefs::kEnterpriseLogoUrl,
      base::BindRepeating(&ManagedMenuView::UpdateProfileManagementIcon,
                          weak_factory_.GetWeakPtr()));

  local_state_change_registrar_.Init(g_browser_process->local_state());
  local_state_change_registrar_.Add(
      prefs::kEnterpriseCustomLabel,
      base::BindRepeating(&ManagedMenuView::RebuildView,
                          weak_factory_.GetWeakPtr()));
  local_state_change_registrar_.Add(
      prefs::kEnterpriseLogoUrl,
      base::BindRepeating(&ManagedMenuView::UpdateBrowserManagementIcon,
                          weak_factory_.GetWeakPtr()));
}

ManagedMenuView::~ManagedMenuView() = default;

void ManagedMenuView::Init() {
  UpdateBrowserManagementIcon();
  UpdateProfileManagementIcon();
}

Profile* ManagedMenuView::GetProfile() const {
  return browser_->profile();
}

void ManagedMenuView::OpenManagementPage() {
  base::RecordAction(base::UserMetricsAction(
      "ManagementPage_OpenedFromManagementBubbleLearnMore"));
  chrome::ShowEnterpriseManagementPageInTabbedBrowser(browser_);
}

void ManagedMenuView::UpdateProfileManagementIcon() {
  enterprise_util::GetManagementIcon(
      GURL(GetProfile()->GetPrefs()->GetString(prefs::kEnterpriseLogoUrl)),
      GetProfile(),
      base::BindOnce(&ManagedMenuView::SetProfileManagementIcon,
                     weak_factory_.GetWeakPtr()));
}

void ManagedMenuView::UpdateBrowserManagementIcon() {
  enterprise_util::GetManagementIcon(
      GURL(g_browser_process->local_state()->GetString(
          prefs::kEnterpriseLogoUrl)),
      GetProfile(),
      base::BindOnce(&ManagedMenuView::SetBrowserManagementIcon,
                     weak_factory_.GetWeakPtr()));
}

void ManagedMenuView::SetProfileManagementIcon(const gfx::Image& icon) {
  profile_management_icon_ = icon;
  RebuildView();
}

void ManagedMenuView::SetBrowserManagementIcon(const gfx::Image& icon) {
  browser_management_icon_ = icon;
  RebuildView();
}

int ManagedMenuView::GetMaxHeight() const {
  gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect screen_space =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  int available_space = screen_space.bottom() - anchor_rect.bottom();
#if BUILDFLAG(IS_WIN)
  // On Windows the bubble can also be shown to the top of the anchor.
  available_space =
      std::max(available_space, anchor_rect.y() - screen_space.y());
#endif
  return std::max(0, available_space);
}

void ManagedMenuView::RebuildView() {
  info_container_ = nullptr;
  inline_title_ = nullptr;
  RemoveAllChildViews();
  BuildView();
}

void ManagedMenuView::BuildView() {
  // Create a table layout to set the menu width.
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(
          views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
          views::TableLayout::kFixedSize,
          views::TableLayout::ColumnSize::kFixed, kContentWidth, kContentWidth)
      .AddRows(1, 1.0f);

  // Create a scroll view to hold the components.
  auto* scroll_view = AddChildView(
      views::Builder<views::ScrollView>()
          .SetHorizontalScrollBarMode(
              views::ScrollView::ScrollBarMode::kDisabled)
          // TODO(crbug.com/41406562): it's a workaround for the crash.
          .SetDrawOverflowIndicator(false)
          .ClipHeightTo(0, GetMaxHeight())
          .Build());

  auto* main_container = scroll_view->SetContents(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetInsideBorderInsets(gfx::Insets::VH(0, 0))
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .Build());

  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(GetProfile());
  bool account_managed = management_service->IsAccountManaged();
  bool device_managed = management_service->IsBrowserManaged();

  // We can show an icon if at least one is available and either only the
  // profile or browser is managed or both are managed by the same entity,
  // meaning they should have the same icon.
  bool show_window_icon =
      (!profile_management_icon_.IsEmpty() ||
       !browser_management_icon_.IsEmpty()) &&
      (account_managed != device_managed ||
       chrome::AreProfileAndBrowserManagedBySameEntity(GetProfile()));

  // The title should be shown on the top bar if no icon is shown, otherwise it
  // should be shown under the icon.
  if (show_window_icon) {
    auto icon = ui::ImageModel::FromImageSkia(
        profiles::GetSizedAvatarIcon(
            profile_management_icon_.IsEmpty() ? browser_management_icon_
                                               : profile_management_icon_,
            kWindowIconSize, kWindowIconSize, profiles::SHAPE_SQUARE)
            .AsImageSkia());
    SetIcon(icon);
    SetShowIcon(true);
    inline_title_ = main_container->AddChildView(
        views::Builder<views::Label>()
            .SetText(chrome::GetManagementBubbleTitle(GetProfile()))
            .SetTextContext(views::style::CONTEXT_LABEL)
            .SetTextStyle(views::style::STYLE_HEADLINE_4)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .SetBorder(views::CreateEmptyBorder(
                gfx::Insets::TLBR(0, 0, kContentGap, 0)))
            .SetMultiLine(true)
            .Build());
    SetTitle(std::u16string());
  } else {
    SetShowIcon(false);
    SetTitle(chrome::GetManagementBubbleTitle(GetProfile()));
  }

  info_container_ = main_container->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetInsideBorderInsets(
              gfx::Insets::VH(kContentMargin, kContentMargin))
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .Build());

  ui::ImageModel profile_management_icon;
  ui::ImageModel browser_management_icon;
  if (account_managed) {
    profile_management_icon =
        profile_management_icon_.IsEmpty()
            ? ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                             ui::kColorMenuIcon, kMenuIconSize)
            : ui::ImageModel::FromImageSkia(
                  profiles::GetSizedAvatarIcon(profile_management_icon_,
                                               kMenuIconSize, kMenuIconSize,
                                               profiles::SHAPE_SQUARE)
                      .AsImageSkia());
  }
  if (device_managed) {
    browser_management_icon =
        browser_management_icon_.IsEmpty()
            ? ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                             ui::kColorMenuIcon, kMenuIconSize)
            : ui::ImageModel::FromImageSkia(
                  profiles::GetSizedAvatarIcon(browser_management_icon_,
                                               kMenuIconSize, kMenuIconSize,
                                               profiles::SHAPE_SQUARE)
                      .AsImageSkia());
  }

  profile_management_label_.clear();
  browser_management_label_.clear();
  if (account_managed && device_managed &&
      !chrome::AreProfileAndBrowserManagedBySameEntity(GetProfile())) {
    auto profile_manager = chrome::GetAccountManagerIdentity(GetProfile());
    CHECK(profile_manager);
    profile_management_label_ =
        l10n_util::GetStringFUTF16(IDS_MANAGEMENT_DIALOG_PROFILE_MANAGED_BY,
                                   base::UTF8ToUTF16(*profile_manager));
    AddDisclaimerSection(info_container_, profile_management_icon,
                         profile_management_label_,
                         /*bottom_marging*/ kContentGap);
    auto browser_manager = chrome::GetDeviceManagerIdentity();
    browser_management_label_ =
        browser_manager && !browser_manager->empty()
            ? l10n_util::GetStringFUTF16(
                  IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY,
                  base::UTF8ToUTF16(*browser_manager))
            : l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED);
    AddDisclaimerSection(info_container_, browser_management_icon,
                         browser_management_label_);
    info_container_->AddChildView(
        views::Builder<views::Separator>()
            .SetColorId(ui::kColorSysDivider)
            .SetOrientation(views::Separator::Orientation::kHorizontal)
            .SetPreferredLength(kContentWidth)
            .SetBorder(
                views::CreateEmptyBorder(gfx::Insets::VH(kContentMargin, 0)))
            .Build());
  }

  // Things to consider section
  info_container_->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_MANAGEMENT_DIALOG_THINGS_TO_CONSIDER_SUBTITLE))
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetTextStyle(views::style::STYLE_BODY_4_MEDIUM)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetBorder(
              views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, kContentGap, 0)))
          .SetMultiLine(true)
          .Build());

  AddDisclaimerSection(
      info_container_,
      ui::ImageModel::FromVectorIcon(vector_icons::kVisibilityIcon,
                                     ui::kColorMenuIcon, kMenuIconSize),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSING_DATA_MANAGEMENT),
      /*bottom_marging*/ kContentGap);
  AddDisclaimerSection(
      info_container_,
      ui::ImageModel::FromVectorIcon(vector_icons::kDevicesIcon,
                                     ui::kColorMenuIcon, kMenuIconSize),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_DEVICE_SIGNALS));

  auto* color_provider = GetColorProvider();
  if (color_provider) {
    BuildInfoContainerBackground(color_provider);
  } else {
    info_container_background_callback_ =
        base::BindRepeating(&ManagedMenuView::BuildInfoContainerBackground,
                            weak_factory_.GetWeakPtr());
  }
  if (GetWidget()) {
    SizeToContents();
  }
}

std::u16string ManagedMenuView::GetAccessibleWindowTitle() const {
  return chrome::GetManagementBubbleTitle(GetProfile());
}

void ManagedMenuView::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  info_container_background_callback_.Run(color_provider);
}

const std::u16string& ManagedMenuView::profile_management_label() const {
  return profile_management_label_;
}

const std::u16string& ManagedMenuView::browser_management_label() const {
  return browser_management_label_;
}

const views::Label* ManagedMenuView::inline_management_title() const {
  return inline_title_;
}

void ManagedMenuView::BuildInfoContainerBackground(
    const ui::ColorProvider* color_provider) {
  const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  info_container_->SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(ui::kColorSysSurface4), radius));
  info_container_->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(
          0, radius, color_provider->GetColor(ui::kColorSysSurface4)),
      gfx::Insets::VH(0, 0)));
}

BEGIN_METADATA(ManagedMenuView)
END_METADATA
