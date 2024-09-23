// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_cross_domain_confirmation_popup_view_views.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_pixel_test.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Values;

constexpr gfx::RectF kElementBounds{100, 100, 250, 50};

class MockPasswordCrossDomainConfirmationPopupController
    : public autofill::AutofillPopupViewDelegate {
 public:
  MockPasswordCrossDomainConfirmationPopupController() = default;
  ~MockPasswordCrossDomainConfirmationPopupController() override = default;

  // AutofillPopupViewDelegate:
  MOCK_METHOD(void, Hide, (autofill::SuggestionHidingReason), (override));
  MOCK_METHOD(void, ViewDestroyed, (), (override));
  MOCK_METHOD(gfx::NativeView, container_view, (), (const override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (const override));
  MOCK_METHOD(const gfx::RectF&, element_bounds, (), (const override));
  MOCK_METHOD(autofill::PopupAnchorType, anchor_type, (), (const override));
  MOCK_METHOD(base::i18n::TextDirection,
              GetElementTextDirection,
              (),
              (const override));

  base::WeakPtr<MockPasswordCrossDomainConfirmationPopupController>
  GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPasswordCrossDomainConfirmationPopupController>
      weak_ptr_factory_{this};
};

}  // namespace

class PasswordCrossDomainConfirmationPopupViewBrowsertest
    : public autofill::PopupPixelTest<
          PasswordCrossDomainConfirmationPopupViewViews,
          MockPasswordCrossDomainConfirmationPopupController> {
 public:
  PasswordCrossDomainConfirmationPopupViewBrowsertest() = default;
  ~PasswordCrossDomainConfirmationPopupViewBrowsertest() override = default;

  void SetUpOnMainThread() override {
    PopupPixelTest::SetUpOnMainThread();

    ON_CALL(controller(), element_bounds())
        .WillByDefault(ReturnRef(kElementBounds));
  }

  void ShowUi(const std::string& name) override {
    PopupPixelTest::ShowUi(name);
    view()->Show();
  }

 protected:
  // autofill::PopupPixelTest:
  PasswordCrossDomainConfirmationPopupViewViews* CreateView(
      MockPasswordCrossDomainConfirmationPopupController& controller) override {
    return new PasswordCrossDomainConfirmationPopupViewViews(
        controller.GetWeakPtr(),
        views::Widget::GetWidgetForNativeWindow(
            browser()->window()->GetNativeWindow()),
        /*domain=*/GURL("https://a.com"),
        /*password_origin=*/u"b.com", base::DoNothing(), base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_P(PasswordCrossDomainConfirmationPopupViewBrowsertest,
                       StaticUI) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordCrossDomainConfirmationPopupViewBrowsertest,
    Combine(/*is_dark_mode=*/Bool(), /*is_rtl=*/Bool()),
    PasswordCrossDomainConfirmationPopupViewBrowsertest::GetTestSuffix);
