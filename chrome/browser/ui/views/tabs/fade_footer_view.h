// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FADE_FOOTER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FADE_FOOTER_VIEW_H_

#include <string>

#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/views/tabs/fade_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

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
  explicit FooterRow(bool is_fade_out_view);
  ~FooterRow() override = default;

  virtual void SetContent(const ui::ImageModel& icon_image_model,
                          std::u16string label_text,
                          int max_footer_width);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // FadeWrapper:
  void SetFade(double percent) override;

 protected:
  views::Label* footer_label() { return footer_label_; }

  views::ImageView* icon() { return icon_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardFooterUpdatesTabAlertStatus);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardFooterShowsDiscardStatus);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardFooterShowsMemoryUsage);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardShowsMemoryOnMemoryRefresh);
  const bool is_fade_out_view_ = false;
  raw_ptr<views::Label> footer_label_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
};

class FadeAlertFooterRow : public FooterRow<AlertFooterRowData> {
 public:
  explicit FadeAlertFooterRow(bool is_fade_out_view)
      : FooterRow<AlertFooterRowData>(is_fade_out_view) {}
  ~FadeAlertFooterRow() override = default;

  // FadeWrapper:
  void SetData(const AlertFooterRowData& data) override;
};

class FadePerformanceFooterRow : public FooterRow<PerformanceRowData> {
 public:
  explicit FadePerformanceFooterRow(bool is_fade_out_view)
      : FooterRow<PerformanceRowData>(is_fade_out_view) {}
  ~FadePerformanceFooterRow() override = default;

  // FadeWrapper:
  void SetData(const PerformanceRowData& data) override;
};

class FooterView : public views::View {
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

  // views::View:
  gfx::Size GetMinimumSize() const override;

 private:
  raw_ptr<views::FlexLayout> flex_layout_ = nullptr;
  raw_ptr<AlertFadeView> alert_row_ = nullptr;
  raw_ptr<PerformanceFadeView> performance_row_ = nullptr;

  void UpdateVisibility();
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FADE_FOOTER_VIEW_H_
