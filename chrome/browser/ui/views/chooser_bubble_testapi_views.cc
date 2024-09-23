// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/bind.h"
#include "chrome/browser/ui/chooser_bubble_testapi.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace test {
namespace {

const char* kViewClassName = "ChooserBubbleUiViewDelegate";

class ChooserBubbleUiWaiterViews : public ChooserBubbleUiWaiter {
 public:
  ChooserBubbleUiWaiterViews()
      : observer_(views::test::AnyWidgetTestPasskey{}) {
    run_loop_.emplace();
    observer_.set_shown_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->GetName() == kViewClassName) {
            has_shown_ = true;
            run_loop_->Quit();
          }
        }));
    observer_.set_closing_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->GetName() == kViewClassName) {
            has_closed_ = true;
            run_loop_->Quit();
          }
        }));
  }

  void WaitForChange() override {
    run_loop_->Run();
    run_loop_.emplace();
  }

 private:
  std::optional<base::RunLoop> run_loop_;
  views::AnyWidgetObserver observer_;
};

}  // namespace

// static
std::unique_ptr<ChooserBubbleUiWaiter> ChooserBubbleUiWaiter::Create() {
  return std::make_unique<ChooserBubbleUiWaiterViews>();
}

}  // namespace test
