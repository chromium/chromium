// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FADE_FOOTER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FADE_FOOTER_VIEW_H_

#include <string>

#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/views/tabs/fade_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

struct AlertFooterRowData {
  std::optional<TabAlertState> alert_state;
  bool should_show_discard_status = false;
  int64_t memory_savings_in_bytes = 0;
};

struct PerformanceRowData {
  bool show_memory_usage = false;
  bool is_high_memory_usage = false;
  int64_t memory_usage_in_bytes = 0;
};

template <typename T>
class FooterRow : public FadeWrapper<views::View, T> {
  using FadeWrapper_View_T = FadeWrapper<views::View, T>;
  METADATA_TEMPLATE_HEADER(FooterRow, FadeWrapper_View_T)

 public:
  explicit FooterRow(bool is_fade_out_view);
  ~FooterRow() override = default;

  virtual void SetContent(const ui::ImageModel& icon_image_model,
                          std::u16string label_text);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;

  // FadeWrapper:
  void SetFade(double percent) override;

  views::Label* footer_label() { return footer_label_; }

  views::ImageView* icon() { return icon_; }

 private:
  const bool is_fade_out_view_ = false;
  raw_ptr<views::Label> footer_label_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
};

using FadeWrapper_View_AlertFooterRowData =
    FadeWrapper<views::View, AlertFooterRowData>;
DECLARE_TEMPLATE_METADATA(FadeWrapper_View_AlertFooterRowData, FadeWrapper);

using FadeWrapper_View_PerformanceRowData =
    FadeWrapper<views::View, PerformanceRowData>;
DECLARE_TEMPLATE_METADATA(FadeWrapper_View_PerformanceRowData, FadeWrapper);

using FooterRow_AlertFooterRowData = FooterRow<AlertFooterRowData>;
DECLARE_TEMPLATE_METADATA(FooterRow_AlertFooterRowData, FooterRow);

using FooterRow_PerformanceRowData = FooterRow<PerformanceRowData>;
DECLARE_TEMPLATE_METADATA(FooterRow_PerformanceRowData, FooterRow);

class FadeAlertFooterRow : public FooterRow<AlertFooterRowData> {
  using FooterRowAlertFooterRowData = FooterRow<AlertFooterRowData>;
  METADATA_HEADER(FadeAlertFooterRow, FooterRowAlertFooterRowData)

 public:
  explicit FadeAlertFooterRow(bool is_fade_out_view)
      : FooterRow<AlertFooterRowData>(is_fade_out_view) {}
  ~FadeAlertFooterRow() override = default;

  // FadeWrapper:
  void SetData(const AlertFooterRowData& data) override;
};

class FadePerformanceFooterRow : public FooterRow<PerformanceRowData> {
  using FooterRowPerformanceRowData = FooterRow<PerformanceRowData>;
  METADATA_HEADER(FadePerformanceFooterRow, FooterRowPerformanceRowData)

 public:
  explicit FadePerformanceFooterRow(bool is_fade_out_view)
      : FooterRow<PerformanceRowData>(is_fade_out_view) {}
  ~FadePerformanceFooterRow() override = default;

  // FadeWrapper:
  void SetData(const PerformanceRowData& data) override;
};

class FooterView : public views::View {
  METADATA_HEADER(FooterView, views::View)

 public:
  using AlertFadeView =
      FadeView<FadeAlertFooterRow, FadeAlertFooterRow, AlertFooterRowData>;
  using PerformanceFadeView = FadeView<FadePerformanceFooterRow,
                                       FadePerformanceFooterRow,
                                       PerformanceRowData>;
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHoverCardFooterElementId);

  FooterView();
  ~FooterView() override = default;

  void SetAlertData(const AlertFooterRowData& data);
  void SetPerformanceData(const PerformanceRowData& data);
  void SetFade(double percent);

  AlertFadeView* GetAlertRowForTesting() { return alert_row_; }

  PerformanceFadeView* GetPerformanceRowForTesting() {
    return performance_row_;
  }

  views::FlexLayout* flex_layout() { return flex_layout_; }

 private:
  raw_ptr<views::FlexLayout> flex_layout_ = nullptr;
  raw_ptr<AlertFadeView> alert_row_ = nullptr;
  raw_ptr<PerformanceFadeView> performance_row_ = nullptr;

  void UpdateVisibility();
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FADE_FOOTER_VIEW_H_
