// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_COMPLETION_SUGGESTION_VIEW_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_COMPLETION_SUGGESTION_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

namespace ui {
namespace ime {

struct SuggestionDetails;
class CompletionSuggestionLabelView;

// Font-related constants
constexpr char kFontStyle[] = "Roboto";
constexpr int kAnnotationFontSize = 10;
constexpr int kIndexFontSize = 10;

// Style-related constants
constexpr int kAnnotationBorderThickness = 1;
constexpr int kAnnotationCornerRadius = 2;
constexpr int kPadding = 8;
constexpr int kAnnotationPaddingLeft = 12;
constexpr int kAnnotationPaddingBottom = 16;
constexpr int kAnnotationPaddingTop = 6;
constexpr char kTabKey[] = "tab";
constexpr cros_styles::ColorName kButtonHighlightColor =
    cros_styles::ColorName::kRippleColor;

// CompletionSuggestionView renders a suggestion.
class UI_CHROMEOS_EXPORT CompletionSuggestionView : public views::Button {
  METADATA_HEADER(CompletionSuggestionView, views::Button)

 public:
  explicit CompletionSuggestionView(PressedCallback callback);
  CompletionSuggestionView(const CompletionSuggestionView&) = delete;
  CompletionSuggestionView& operator=(const CompletionSuggestionView&) = delete;
  ~CompletionSuggestionView() override;

  void SetView(const SuggestionDetails& details);

  void SetHighlighted(bool highlighted);
  void SetMinWidth(int width);

  // When this view is being anchored to some other view, returns the point in
  // this view that this should be anchored to, in local coordinates.
  // For example, if this method returns the bottom left corner of this view,
  // then this view should be placed above the anchor so that the bottom left
  // corner of this view corresponds to the anchor.
  gfx::Point GetAnchorOrigin() const;

  std::u16string GetSuggestionForTesting();
  CompletionSuggestionLabelView* suggestion_label_for_testing() const;

 private:
  friend class SuggestionWindowViewTest;
  FRIEND_TEST_ALL_PREFIXES(SuggestionWindowViewTest, ShortcutSettingTest);

  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;

  std::unique_ptr<views::View> CreateAnnotationContainer();
  std::unique_ptr<views::View> CreateDownAndEnterAnnotationLabel();
  std::unique_ptr<views::View> CreateTabAnnotationLabel();

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.

  void SetSuggestionText(const std::u16string& text,
                         const size_t confirmed_length);

  // The suggestion label renders the suggestion text.
  raw_ptr<CompletionSuggestionLabelView> suggestion_label_ = nullptr;
  // The annotation view renders annotations.
  raw_ptr<views::View> annotation_container_ = nullptr;
  raw_ptr<views::View> down_and_enter_annotation_label_ = nullptr;
  raw_ptr<views::View> tab_annotation_label_ = nullptr;
  raw_ptr<views::ImageView> down_icon_ = nullptr;
  raw_ptr<views::ImageView> arrow_icon_ = nullptr;

  int suggestion_width_ = 0;
  int min_width_ = 0;
  bool highlighted_ = false;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT, CompletionSuggestionView, views::Button)
VIEW_BUILDER_PROPERTY(const SuggestionDetails&, View)
VIEW_BUILDER_PROPERTY(bool, Highlighted)
VIEW_BUILDER_PROPERTY(int, MinWidth)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::CompletionSuggestionView)

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_COMPLETION_SUGGESTION_VIEW_H_
