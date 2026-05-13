// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/metrics/metrics_reporting_level.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/widget.h"

namespace {

class TestApi : public ui::DialogModelFieldHost {
 public:
  explicit TestApi(ui::DialogModel* model) : model_(model) {}

  void SetUmaConsentCheckboxChecked(bool checked) {
    model_
        ->GetCheckboxByUniqueId(
            SessionCrashedBubbleView::GetUmaConsentCheckboxIdForTesting())
        ->OnChecked(GetPassKey(), checked);
  }

  void SetChangeMetricsReportingStateCallbackForTesting(
      SessionCrashedBubbleView::ChangeMetricsReportingStateCallback callback) {
    SessionCrashedBubbleView::SetChangeMetricsReportingStateCallbackForTesting(
        model_, std::move(callback));
  }

 private:
  const raw_ptr<ui::DialogModel> model_;
};

}  // namespace

class SessionCrashedBubbleViewRestructureTest : public DialogBrowserTest {
 public:
  SessionCrashedBubbleViewRestructureTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    SessionCrashedBubbleView::ShowBubble(browser(), /*offer_uma_optin=*/true);
  }
};

IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewRestructureTest,
                       AcceptSetsBasicLevel) {
  ShowUi("AcceptSetsBasicLevel");
  views::BubbleDialogModelHost* model_host =
      SessionCrashedBubbleView::GetModelHostForTesting();
  ASSERT_NE(model_host, nullptr);
  views::Widget* dialog_widget = model_host->GetWidget();
  ASSERT_NE(dialog_widget, nullptr);

  base::test::TestFuture<metrics::MetricsReportingLevel> future;
  {
    TestApi test_api(model_host->GetModelForTesting());
    test_api.SetUmaConsentCheckboxChecked(true);
    test_api.SetChangeMetricsReportingStateCallbackForTesting(
        future.GetRepeatingCallback());
  }

  views::test::AcceptDialog(dialog_widget);
  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic, future.Get<0>());
}
