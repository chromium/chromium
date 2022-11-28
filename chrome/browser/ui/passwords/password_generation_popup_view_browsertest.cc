// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_view.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/password_manager_uitest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view_tester.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

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

}  // namespace autofill
