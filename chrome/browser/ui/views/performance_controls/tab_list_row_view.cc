// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/tab_list_row_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/ui/performance_controls/tab_list_model.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace {
// Spacing placed on top and below the favicon so that the favicon border
// won't touch the edge of the TabListRowView.
constexpr int kFaviconVerticalMargin = 4;
// Corner radius for the favicon image view.
constexpr int kFaviconCornerRadius = 4;
// Border thickness surrounding the favicon.
constexpr int kFaviconBorderThickness = 4;
// Spacing between the favicon and tab title.
constexpr int kFaviconTabTitleSpacing = 8;

std::unique_ptr<views::Label> CreateLabel(std::u16string text, int text_style) {
  auto label = std::make_unique<views::Label>(text);

  label->SetMultiLine(false);
  label->SetMaxLines(1);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetTextStyle(text_style);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded, true));

  return label;
}
}  // namespace

class TextContainer : public views::View {
  METADATA_HEADER(TextContainer, views::View)
 public:
  TextContainer(std::u16string title,
                GURL domain,
                TabListModel* model,
                base::RepeatingClosure on_reverse_focus_tab_traversal)
      : tab_list_model_(model),
        on_reverse_focus_tab_traversal_(on_reverse_focus_tab_traversal) {
    views::FlexLayout* const flex_layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    flex_layout->SetOrientation(views::LayoutOrientation::kVertical);
    flex_layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);

    title_ =
        AddChildView(CreateLabel(title, views::style::STYLE_BODY_4_MEDIUM));

    SetFocusBehavior(views::PlatformStyle::kDefaultFocusBehavior);

    GetViewAccessibility().SetRole(ax::mojom::Role::kListBoxOption);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    if (tab_list_model_->count() > 1) {
      node_data->SetNameChecked(title_->GetText());
    } else {
      node_data->SetNameChecked(base::StrCat(
          {title_->GetText(), u" ",
           l10n_util::GetStringUTF16(
               IDS_PERFORMANCE_INTERVENTION_SINGLE_SUGGESTED_ROW_ACCNAME)}));
    }
  }

  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    if (reverse) {
      on_reverse_focus_tab_traversal_.Run();
    }
  }

  views::Label* title() { return title_; }

 private:
  raw_ptr<TabListModel> tab_list_model_;
  base::RepeatingClosure on_reverse_focus_tab_traversal_;
  raw_ptr<views::Label> title_ = nullptr;
};

BEGIN_METADATA(TextContainer)
END_METADATA

TabListRowView::TabListRowView(
    resource_attribution::PageContext tab,
    TabListModel* tab_list_model,
    base::OnceCallback<void(TabListRowView*)> close_button_callback)
    : actionable_tab_(tab),
      tab_list_model_(tab_list_model),
      inkdrop_container_(
          AddChildView(std::make_unique<views::InkDropContainerView>())) {
  views::FlexLayout* const flex_layout =
      views::View::SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kHorizontal);

  auto ink_drop_host_unique = std::make_unique<views::InkDropHost>(this);
  views::InkDropHost* ink_drop_host = ink_drop_host_unique.get();
  views::InkDrop::Install(this, std::move(ink_drop_host_unique));
  views::InstallRectHighlightPathGenerator(this);
  ink_drop_host->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop_host->SetBaseColorId(ui::kColorSysStateHoverOnSubtle);
  ink_drop_host->SetHighlightOpacity(1.0f);
  ink_drop_host->GetInkDrop()->SetHoverHighlightFadeDuration(base::TimeDelta());
  ink_drop_host->GetInkDrop()->SetShowHighlightOnFocus(true);

  content::WebContents* const web_contents = tab.GetWebContents();
  CHECK(web_contents);

  TabUIHelper* const tab_ui_helper = TabUIHelper::FromWebContents(web_contents);
  CHECK(tab_ui_helper);

  // The container adds all contents of the row as child views to ensure that we
  // have consistent margin space on the left and right side of the row and the
  // hover highlighting effects goes across the entire row.
  views::View* row_container = AddChildView(std::make_unique<views::View>());
  // Use the dialog bubble's insets to give the left and right ends of the
  // container some space away from the edge of row view.
  views::LayoutProvider* const layout_provider = views::LayoutProvider::Get();
  CHECK(layout_provider);
  gfx::Insets side_insets =
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);
  side_insets.set_top(0);
  side_insets.set_bottom(0);
  row_container->SetProperty(views::kMarginsKey, side_insets);
  row_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  views::ImageView* const favicon = row_container->AddChildView(
      std::make_unique<views::ImageView>(tab_ui_helper->GetFavicon()));

  favicon->SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorSysNeutralContainer, kFaviconCornerRadius));
  favicon->SetBorder(views::CreateThemedRoundedRectBorder(
      kFaviconBorderThickness, kFaviconCornerRadius,
      ui::kColorSysNeutralContainer));

  favicon->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kFaviconVerticalMargin, 0, kFaviconVerticalMargin,
                        kFaviconTabTitleSpacing));

  // Unretained(this) is safe to use in this instance because the callback is
  // owned by the text container which is a descendant of this. Therefore the
  // callback will not be invoked after this is destroyed.
  text_container_ = row_container->AddChildView(std::make_unique<TextContainer>(
      tab_ui_helper->GetTitle(), web_contents->GetLastCommittedURL(),
      tab_list_model_,
      base::BindRepeating(&TabListRowView::MaybeFocusCloseButton,
                          base::Unretained(this))));

  std::unique_ptr<views::ImageButton> close_button =
      views::CreateVectorImageButtonWithNativeTheme(
          base::BindOnce(std::move(close_button_callback), this),
          vector_icons::kCloseChromeRefreshIcon);

  // The close button should not be visible by default and should show up when
  // the user's mouse is over TabListRowView.
  close_button->SetVisible(false);
  close_button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));
  views::InstallCircleHighlightPathGenerator(close_button.get());
  close_button->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_PERFORMANCE_INTERVENTION_CLOSE_BUTTON_ACCNAME,
      text_container_->title()->GetText()));
  close_button_ = row_container->AddChildView(std::move(close_button));

  inkdrop_container_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  SetNotifyEnterExitOnChild(true);
  tab_list_model_observation_.Observe(tab_list_model_);
}

TabListRowView::~TabListRowView() {
  // Explicit removal of InkDrop for classes that override
  // Add/RemoveLayerFromRegions(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

std::u16string TabListRowView::GetTitleTextForTesting() {
  return text_container_->title()->GetText();
}

views::ImageButton* TabListRowView::GetCloseButtonForTesting() {
  return close_button_;
}

views::View* TabListRowView::GetTextContainerForTesting() {
  return text_container_;
}

void TabListRowView::OnMouseEntered(const ui::MouseEvent& event) {
  views::View::OnMouseEntered(event);
  RefreshInkDropAndCloseButton();
}

void TabListRowView::OnMouseExited(const ui::MouseEvent& event) {
  View::OnMouseExited(event);
  RefreshInkDropAndCloseButton();
}

void TabListRowView::AddLayerToRegion(ui::Layer* layer,
                                      views::LayerRegion region) {
  inkdrop_container_->AddLayerToRegion(layer, region);
}

void TabListRowView::RemoveLayerFromRegions(ui::Layer* layer) {
  inkdrop_container_->RemoveLayerFromRegions(layer);
}

void TabListRowView::AddedToWidget() {
  auto* const focus_manager = GetFocusManager();
  CHECK(focus_manager);
  focus_manager->AddFocusChangeListener(this);
}

void TabListRowView::RemovedFromWidget() {
  auto* const focus_manager = GetFocusManager();
  CHECK(focus_manager);
  focus_manager->RemoveFocusChangeListener(this);
}

void TabListRowView::OnDidChangeFocus(views::View* before, views::View* now) {
  if (now) {
    RefreshInkDropAndCloseButton();
  }
}

void TabListRowView::OnTabCountChanged(int count) {
  if (count <= 1) {
    RefreshInkDropAndCloseButton();
  }
}

void TabListRowView::RefreshInkDropAndCloseButton() {
  const bool has_focus =
      text_container_->HasFocus() || close_button_->HasFocus();
  const bool showing_multiple_rows =
      tab_list_model_->page_contexts().size() > 1;
  if (!showing_multiple_rows) {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
  }
  close_button_->SetVisible(showing_multiple_rows &&
                            (has_focus || IsMouseHovered()));
  views::InkDrop::Get(this)->GetInkDrop()->SetFocused(showing_multiple_rows &&
                                                      has_focus);
}

void TabListRowView::MaybeFocusCloseButton() {
  // Update focus only if focus received from a different row.
  if (GetFocusManager() && !Contains(GetFocusManager()->GetFocusedView())) {
    close_button_->SetVisible(true);
    close_button_->RequestFocus();
  }
}

BEGIN_METADATA(TabListRowView)
END_METADATA
