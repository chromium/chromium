// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"

#include <stddef.h>

#include <memory>

#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {

namespace {

class TestAutofillPopupViewDelegate : public AutofillPopupViewDelegate {
 public:
  explicit TestAutofillPopupViewDelegate(content::WebContents* web_contents)
      : element_bounds_(0.0, 0.0, 100.0, 100.0),
        container_view_(web_contents->GetNativeView()) {}

  void Hide() override {}
  void ViewDestroyed() override {}
  void SetSelectionAtPoint(const gfx::Point& point) override {}
  bool AcceptSelectedLine() override { return true; }
  void SelectionCleared() override {}
  bool HasSelection() const override { return false; }
  gfx::Rect popup_bounds() const override { return gfx::Rect(0, 0, 100, 100); }
  gfx::NativeView container_view() const override { return container_view_; }
  const gfx::RectF& element_bounds() const override { return element_bounds_; }
  bool IsRTL() const override { return false; }

  const std::vector<autofill::Suggestion> GetSuggestions() override {
    // Give elements 1 and 3 subtexts and elements 2 and 3 icons, to ensure
    // all combinations of subtexts and icons.
    std::vector<Suggestion> suggestions;
    suggestions.push_back(Suggestion("", "", "", 0));
    suggestions.push_back(Suggestion("", "x", "", 0));
    suggestions.push_back(Suggestion("", "", "americanExpressCC", 0));
    suggestions.push_back(Suggestion("", "x", "genericCC", 0));
    return suggestions;
  }
#if !defined(OS_ANDROID)
  int GetElidedValueWidthForRow(int row) override { return 0; }
  int GetElidedLabelWidthForRow(int row) override { return 0; }
#endif

 private:
  gfx::RectF element_bounds_;
  gfx::NativeView container_view_;
};

class AutofillPopupLayoutModelTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    delegate_ = std::make_unique<TestAutofillPopupViewDelegate>(web_contents());
    layout_model_ = std::make_unique<AutofillPopupLayoutModel>(
        delegate_.get(), false /* is_credit_card_field */);
  }

  AutofillPopupLayoutModel* layout_model() { return layout_model_.get(); }

 private:
  std::unique_ptr<TestAutofillPopupViewDelegate> delegate_;
  std::unique_ptr<AutofillPopupLayoutModel> layout_model_;
};

}  // namespace

#if !defined(OS_ANDROID)
TEST_F(AutofillPopupLayoutModelTest, RowWidthWithoutText) {
  int base_size =
      AutofillPopupLayoutModel::kEndPadding * 2 + kPopupBorderThickness * 2;
  int subtext_increase = AutofillPopupLayoutModel::kNamePadding;

  // Refer to GetSuggestions() in TestAutofillPopupViewDelegate.
  EXPECT_EQ(base_size,
            layout_model()->RowWidthWithoutText(0, /* has_substext= */ false));
  EXPECT_EQ(base_size + subtext_increase,
            layout_model()->RowWidthWithoutText(1, /* has_substext= */ true));
  EXPECT_EQ(base_size + AutofillPopupLayoutModel::kIconPadding +
                ui::ResourceBundle::GetSharedInstance()
                    .GetImageNamed(IDR_AUTOFILL_CC_AMEX)
                    .Width(),
            layout_model()->RowWidthWithoutText(2, /* has_substext= */ false));
  EXPECT_EQ(base_size + subtext_increase +
                AutofillPopupLayoutModel::kIconPadding +
                ui::ResourceBundle::GetSharedInstance()
                    .GetImageNamed(IDR_AUTOFILL_CC_GENERIC)
                    .Width(),
            layout_model()->RowWidthWithoutText(3, /* has_substext= */ true));
}
#endif

}  // namespace autofill
