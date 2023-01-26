// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_view.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/password_manager_uitest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view_tester.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/child_iterator_base.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

ui::AXPlatformNodeDelegate* FindNode(ui::AXPlatformNodeDelegate* root,
                                     const std::string& class_name) {
  if (!root) {
    return nullptr;
  }

  if (root->GetStringAttribute(ax::mojom::StringAttribute::kClassName) ==
      class_name) {
    return root;
  }

  for (auto it = root->ChildrenBegin(); *it != *root->ChildrenEnd(); ++(*it)) {
    ui::AXPlatformNodeDelegate* child_found = FindNode(it->get(), class_name);
    if (child_found) {
      return child_found;
    }
  }

  return nullptr;
}

}  // namespace

class PasswordGenerationPopupViewTest : public InProcessBrowserTest {};

class PasswordGenerationPopupViewWithStrengthIndicatorTest
    : public InProcessBrowserTest {
 public:
  PasswordGenerationPopupViewWithStrengthIndicatorTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{password_manager::features::
                                  kPasswordStrengthIndicator},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PasswordGenerationPopupViewWithMinimizedStrengthIndicatorTest
    : public InProcessBrowserTest {
 public:
  PasswordGenerationPopupViewWithMinimizedStrengthIndicatorTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{password_manager::features::
                                   kPasswordStrengthIndicator,
                               {{"strength_indicator_minimized", "true"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for crbug.com/400543. Verifying that moving the mouse in the
// editing dialog doesn't crash.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       MouseMovementInEditingPopup) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(web_contents->GetContainerBounds().x(),
                 web_contents->GetContainerBounds().y(), 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          /*observer=*/nullptr, web_contents,
          web_contents->GetPrimaryMainFrame());

  controller->Show(PasswordGenerationPopupController::kEditGeneratedPassword);
  EXPECT_TRUE(controller->IsVisible());

  PasswordGenerationPopupViewTester::For(controller->view())
      ->SimulateMouseMovementAt(
          gfx::Point(web_contents->GetContainerBounds().x() + 1,
                     web_contents->GetContainerBounds().y() + 1));

  // This hides the popup and destroys the controller.
  web_contents->Close();
}

// Verify that destroying web contents with visible popup does not crash.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       CloseWebContentsWithVisiblePopup) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(web_contents->GetContainerBounds().x(),
                 web_contents->GetContainerBounds().y(), 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          /*observer=*/nullptr, web_contents,
          web_contents->GetPrimaryMainFrame());

  controller->Show(PasswordGenerationPopupController::kEditGeneratedPassword);
  EXPECT_TRUE(controller->IsVisible());

  web_contents->Close();
}

// Verify that controller is not crashed in case of insufficient vertical space
// for showing popup.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       DoNotCrashInCaseOfInsuffucientVerticalSpace) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(web_contents->GetContainerBounds().x(),
                 web_contents->GetContainerBounds().y() - 20, 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          /*observer=*/nullptr, web_contents,
          web_contents->GetPrimaryMainFrame());

  controller->Show(PasswordGenerationPopupController::kEditGeneratedPassword);
  // Check that the object `controller` points to was invalidated.
  EXPECT_FALSE(controller);
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewWithStrengthIndicatorTest,
                       ShowsPopupWithEmptyPasswordField) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(web_contents->GetContainerBounds().x(),
                 web_contents->GetContainerBounds().y(), 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          /*observer=*/nullptr, web_contents,
          web_contents->GetPrimaryMainFrame());

  EXPECT_FALSE(controller->IsVisible());
  controller->UpdatePopupBasedOnTypedPasswordStrength();
  EXPECT_TRUE(controller->IsVisible());

  web_contents->Close();
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewWithStrengthIndicatorTest,
                       ShowsPopupWithWeakPasswordTyped) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(web_contents->GetContainerBounds().x(),
                 web_contents->GetContainerBounds().y(), 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  TestGenerationPopupObserver observer;
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          &observer, web_contents, web_contents->GetPrimaryMainFrame());

  EXPECT_FALSE(controller->IsVisible());
  controller->UpdateTypedPassword(u"weak");
  controller->UpdatePopupBasedOnTypedPasswordStrength();
  observer.WaitForStatus(TestGenerationPopupObserver::GenerationPopup::kShown);
  EXPECT_TRUE(controller->IsVisible());

  web_contents->Close();
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewWithStrengthIndicatorTest,
                       HidesPopupWithStrongPasswordTyped) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(web_contents->GetContainerBounds().x(),
                 web_contents->GetContainerBounds().y(), 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  TestGenerationPopupObserver observer;
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          &observer, web_contents, web_contents->GetPrimaryMainFrame());

  // Make the popup visible first.
  EXPECT_FALSE(controller->IsVisible());
  controller->UpdatePopupBasedOnTypedPasswordStrength();
  EXPECT_TRUE(controller->IsVisible());

  // Popup should be hidden and controller destroyed with strong password typed.
  controller->UpdateTypedPassword(u"fnxsr4@cm^mdls#fkbhisg3d");
  controller->UpdatePopupBasedOnTypedPasswordStrength();
  observer.WaitForStatus(TestGenerationPopupObserver::GenerationPopup::kHidden);
  EXPECT_FALSE(controller);
}

IN_PROC_BROWSER_TEST_F(
    PasswordGenerationPopupViewWithStrengthIndicatorTest,
    ShowsFullPopupWithWeakPasswordTypedInNonMinimizedExperiment) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(web_contents->GetContainerBounds().x(),
                 web_contents->GetContainerBounds().y(), 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  TestGenerationPopupObserver observer;
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          &observer, web_contents, web_contents->GetPrimaryMainFrame());

  EXPECT_FALSE(controller->IsVisible());
  controller->UpdateTypedPassword(u"weak123");
  controller->UpdatePopupBasedOnTypedPasswordStrength();
  observer.WaitForStatus(TestGenerationPopupObserver::GenerationPopup::kShown);
  EXPECT_TRUE(controller->IsVisible());
  EXPECT_FALSE(PasswordGenerationPopupViewTester::For(controller->view())
                   ->IsPopupMinimized());

  web_contents->Close();
}

IN_PROC_BROWSER_TEST_F(
    PasswordGenerationPopupViewWithMinimizedStrengthIndicatorTest,
    ShowsFullPopupWithMoreThanFiveCharWeakPasswordTyped) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(web_contents->GetContainerBounds().x(),
                 web_contents->GetContainerBounds().y(), 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  TestGenerationPopupObserver observer;
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          &observer, web_contents, web_contents->GetPrimaryMainFrame());

  EXPECT_FALSE(controller->IsVisible());
  controller->UpdateTypedPassword(u"weak12");
  controller->UpdatePopupBasedOnTypedPasswordStrength();
  observer.WaitForStatus(TestGenerationPopupObserver::GenerationPopup::kShown);
  EXPECT_TRUE(controller->IsVisible());
  EXPECT_TRUE(PasswordGenerationPopupViewTester::For(controller->view())
                  ->IsPopupMinimized());

  web_contents->Close();
}

IN_PROC_BROWSER_TEST_F(
    PasswordGenerationPopupViewWithMinimizedStrengthIndicatorTest,
    ShowsFullPopupWithFiveCharWeakPasswordTyped) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(web_contents->GetContainerBounds().x(),
                 web_contents->GetContainerBounds().y(), 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  TestGenerationPopupObserver observer;
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          &observer, web_contents, web_contents->GetPrimaryMainFrame());

  EXPECT_FALSE(controller->IsVisible());
  controller->UpdateTypedPassword(u"weak1");
  controller->UpdatePopupBasedOnTypedPasswordStrength();
  observer.WaitForStatus(TestGenerationPopupObserver::GenerationPopup::kShown);
  EXPECT_TRUE(controller->IsVisible());
  EXPECT_FALSE(PasswordGenerationPopupViewTester::For(controller->view())
                   ->IsPopupMinimized());

  web_contents->Close();
}

using PasswordGenerationPopupViewAxTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewAxTest, PopupInAxTree) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto container_bounds = web_contents->GetContainerBounds();
  password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(container_bounds.x(), container_bounds.y(), 10, 10),
      /*max_length=*/10,
      /*generation_element=*/std::u16string(),
      /*user_typed_password=*/std::u16string(), FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      FormData());

  TestGenerationPopupObserver observer;
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data,
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(web_contents)
                  ->GetDriverForFrame(web_contents->GetPrimaryMainFrame())
                  ->AsWeakPtr(),
          &observer, web_contents, web_contents->GetPrimaryMainFrame());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "PasswordGenerationPopupViewViews");
  controller->Show(GenerationUIState::kOfferGeneration);

  gfx::NativeWindow window = gfx::kNullNativeWindow;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // On Mac and Linux the whole ax tree grows from the main root windows
  // and the popup node can be found there. This gives more confidence
  // that it is in the right place than on Windows (see below) where
  // the popup subtree lives separately.
  waiter.WaitIfNeededAndGet();
  window = chrome::FindLastActive()->window()->GetNativeWindow();
#elif BUILDFLAG(IS_WIN)
  views::Widget* dialog_widget = waiter.WaitIfNeededAndGet();
  window = dialog_widget->GetNativeWindow();
#endif

  ui::AXPlatformNode* root_node = ui::AXPlatformNode::FromNativeWindow(window);
  ui::AXPlatformNodeDelegate* root_node_delegate = root_node->GetDelegate();
  ui::AXPlatformNodeDelegate* node_delegate =
      FindNode(root_node_delegate,
               "PasswordGenerationPopupViewViews::GeneratedPasswordBox");

  ASSERT_THAT(node_delegate, ::testing::NotNull());
  EXPECT_FALSE(
      node_delegate->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Set the screen reader focus by calling a method on the controller directly,
  // it normally is triggered by UI events when the screen reader is on,
  // screen reader presence is hard/expensive to emulate.
  static_cast<PasswordGenerationPopupController*>(controller.get())
      ->SetSelected();

  EXPECT_TRUE(
      node_delegate->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  web_contents->Close();
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
#else
  GTEST_SKIP() << "Accessibility reflection is not supported on this platform.";
#endif
}

}  // namespace autofill
