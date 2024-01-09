// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_iban_bottom_sheet_bridge.h"
#include <memory>
#include <string>

#include "base/android/jni_string.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/android/autofill/autofill_save_iban_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using SaveIbanOfferUserDecision = AutofillClient::SaveIbanOfferUserDecision;

std::u16string_view kUserProvidedNickname = u"My Doctor's IBAN";

class AutofillSaveIbanBottomSheetBridgeTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void LocalCallback(SaveIbanOfferUserDecision decision,
                     std::u16string_view nickname) {
    base::DoNothing();
  }

  AutofillClient::SaveIbanPromptCallback MakeLocalCallback() {
    return base::BindOnce(&AutofillSaveIbanBottomSheetBridgeTest::LocalCallback,
                          base::Unretained(this));
  }
};

class MockDelegate : public AutofillSaveIbanDelegate {
 public:
  explicit MockDelegate(
      AutofillClient::SaveIbanPromptCallback save_iban_callback,
      content::WebContents* web_contents)
      : AutofillSaveIbanDelegate(std::move(save_iban_callback), web_contents) {}
  MOCK_METHOD(void, OnUiAccepted, (base::OnceClosure, std::u16string_view));
  MOCK_METHOD(void, OnUiCanceled, ());
  MOCK_METHOD(void, OnUiIgnored, ());
};

// Check OnUiAccepted() is called in delegate.
TEST_F(AutofillSaveIbanBottomSheetBridgeTest, BridgeCallsOnUiAccepted) {
  std::unique_ptr<MockDelegate> delegate =
      std::make_unique<MockDelegate>(MakeLocalCallback(), web_contents());
  MockDelegate& delegate_reference = *delegate;
  AutofillSaveIbanBottomSheetBridge bridge;
  bridge.RequestShowContent(/*iban_label=*/u"", std::move(delegate));

  base::MockOnceClosure mock_accept_callback;
  EXPECT_CALL(delegate_reference, OnUiAccepted);

  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_string =
      base::android::ConvertUTF16ToJavaString(env, kUserProvidedNickname);
  base::android::JavaParamRef<jstring> user_provided_nickname(env,
                                                              j_string.obj());

  bridge.OnUiAccepted(env, user_provided_nickname);
}

// Check OnUiCanceled() is called in delegate.
TEST_F(AutofillSaveIbanBottomSheetBridgeTest, BridgeCallsOnUiCanceled) {
  std::unique_ptr<MockDelegate> delegate =
      std::make_unique<MockDelegate>(MakeLocalCallback(), web_contents());
  MockDelegate& delegate_reference = *delegate;
  AutofillSaveIbanBottomSheetBridge bridge;
  bridge.RequestShowContent(/*iban_label=*/u"", std::move(delegate));

  EXPECT_CALL(delegate_reference, OnUiCanceled());

  bridge.OnUiCanceled(/*env=*/nullptr);
}

// Check OnUiIgnored() is called in delegate.
TEST_F(AutofillSaveIbanBottomSheetBridgeTest, BridgeCallsOnUiIgnored) {
  std::unique_ptr<MockDelegate> delegate =
      std::make_unique<MockDelegate>(MakeLocalCallback(), web_contents());
  MockDelegate& delegate_reference = *delegate;
  AutofillSaveIbanBottomSheetBridge bridge;
  bridge.RequestShowContent(/*iban_label=*/u"", std::move(delegate));

  EXPECT_CALL(delegate_reference, OnUiIgnored());

  bridge.OnUiIgnored(/*env=*/nullptr);
}

}  // namespace
}  // namespace autofill
