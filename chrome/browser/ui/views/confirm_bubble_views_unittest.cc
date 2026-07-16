// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_views.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/test/test_confirm_bubble_model.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

using views::Widget;

typedef ChromeViewsTestBase ConfirmBubbleViewsTest;

// TODO(crbug.com/40099109) Disabled on windows due to flake
#if BUILDFLAG(IS_WIN)
#define MAYBE_CreateAndClose DISABLED_CreateAndClose
#else
#define MAYBE_CreateAndClose CreateAndClose
#endif
TEST_F(ConfirmBubbleViewsTest, MAYBE_CreateAndClose) {
  SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

  // Create parent widget, as confirm bubble must have an owner.
  Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW);
  std::unique_ptr<views::Widget> parent_widget(new Widget);
  parent_widget->Init(std::move(params));
  parent_widget->Show();

  // Bubble owns the model.
  bool model_deleted = false;
  std::unique_ptr<TestConfirmBubbleModel> model(
      new TestConfirmBubbleModel(&model_deleted, nullptr, nullptr, nullptr));
  ConfirmBubbleViews* bubble = new ConfirmBubbleViews(std::move(model));
  gfx::NativeWindow parent = parent_widget->GetNativeWindow();
  constrained_window::CreateBrowserModalDialogViews(bubble, parent)->Show();

  // Clean up.
  bubble->GetWidget()->CloseNow();
  parent_widget->CloseNow();
  EXPECT_TRUE(model_deleted);

  constrained_window::SetConstrainedWindowViewsClient(nullptr);
}
