// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FADE_FOOTER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FADE_FOOTER_VIEW_H_

#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/fade_view.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace {
constexpr int kIconLabelSpacing = 8;
constexpr int kFooterVerticalMargins = 8;
constexpr int kFooterHorizontalMargins = 12;
constexpr auto kFooterMargins =
    gfx::Insets::VH(kFooterVerticalMargins, kFooterHorizontalMargins);
}  // namespace

struct AlertFooterRowData {
  absl::optional<TabAlertState> alert_state;
  int footer_row_width = 0;
};

struct PerformanceRowData {
  bool should_show_discard_status = false;
  uint64_t memory_savings_in_bytes = 0;
  uint64_t memory_usage_in_bytes = 0;
  int footer_row_width = 0;
};

template <typename T>
class FooterRow : public FadeWrapper<views::View, T> {
 public:
  FooterRow();
  ~FooterRow() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // FadeWrapper:
  void SetFade(double percent) override;

 protected:
  views::Label* footer_label() { return footer_label_; }

  views::ImageView* icon() { return icon_; }

  void UpdateIconAndLabelLayout(int max_footer_width);

 private:
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardInteractiveUiTest,
                           HoverCardFooterUpdates);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardInteractiveUiTest,
                           HoverCardFooterShowsDiscardStatus);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardInteractiveUiTest,
                           HoverCardFooterShowsMemoryUsage);
  raw_ptr<views::Label> footer_label_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
};

class FadeAlertFooterRow : public FooterRow<AlertFooterRowData> {
 public:
  FadeAlertFooterRow() = default;
  ~FadeAlertFooterRow() override = default;

  // FadeWrapper:
  void SetData(const AlertFooterRowData& data) override;
};

class FadePerformanceFooterRow : public FooterRow<PerformanceRowData> {
 public:
  FadePerformanceFooterRow() = default;
  ~FadePerformanceFooterRow() override = default;

  // FadeWrapper:
  void SetData(const PerformanceRowData& data) override;
};

class FooterView : public views::View {
 public:
  FooterView() {
    flex_layout_ =
        views::View::SetLayoutManager(std::make_unique<views::FlexLayout>());
    flex_layout_->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCollapseMargins(true)
        .SetInteriorMargin(kFooterMargins)
        .SetDefault(views::kMarginsKey,
                    gfx::Insets::VH(kFooterVerticalMargins, 0));
    alert_row_ = AddChildView(
        std::make_unique<FadeView<FadeAlertFooterRow, FadeAlertFooterRow,
                                  AlertFooterRowData>>(
            std::make_unique<FadeAlertFooterRow>(),
            std::make_unique<FadeAlertFooterRow>()));

    performance_row_ =
        AddChildView(std::make_unique<
                     FadeView<FadePerformanceFooterRow,
                              FadePerformanceFooterRow, PerformanceRowData>>(
            std::make_unique<FadePerformanceFooterRow>(),
            std::make_unique<FadePerformanceFooterRow>()));
  }

  FadeView<FadeAlertFooterRow, FadeAlertFooterRow, AlertFooterRowData>*
  GetAlertRow() {
    return alert_row_;
  }

  FadeView<FadePerformanceFooterRow,
           FadePerformanceFooterRow,
           PerformanceRowData>*
  GetPerformanceRow() {
    return performance_row_;
  }

  // views::View
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  raw_ptr<views::FlexLayout> flex_layout_ = nullptr;
  raw_ptr<FadeView<FadeAlertFooterRow, FadeAlertFooterRow, AlertFooterRowData>>
      alert_row_ = nullptr;
  raw_ptr<FadeView<FadePerformanceFooterRow,
                   FadePerformanceFooterRow,
                   PerformanceRowData>>
      performance_row_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FADE_FOOTER_VIEW_H_
