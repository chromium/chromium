// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_handler.h"

#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using notice::mojom::PrivacySandboxNotice;
using notice::mojom::PrivacySandboxNoticeEvent;

// These constants represent arbitrary notice and event types for testing.
constexpr PrivacySandboxNotice kTestNotice =
    PrivacySandboxNotice::kTopicsConsentNotice;
constexpr PrivacySandboxNoticeEvent kTestEvent =
    PrivacySandboxNoticeEvent::kAck;

// Mock implementation for the BaseDialogUIDelegate interface.
class MockBaseDialogUIDelegate : public BaseDialogUIDelegate {
 public:
  ~MockBaseDialogUIDelegate() override = default;

  MOCK_METHOD(void, ResizeNativeView, (int height), (override));
  MOCK_METHOD(void, ShowNativeView, (), (override));
  MOCK_METHOD(void, CloseNativeView, (), (override));
  MOCK_METHOD(PrivacySandboxNotice, GetPrivacySandboxNotice, (), (override));
  MOCK_METHOD(void,
              SetPrivacySandboxNotice,
              (PrivacySandboxNotice),
              (override));
};

class MockBaseDialogPage : public dialog::mojom::BaseDialogPage {
 public:
  ~MockBaseDialogPage() override = default;

  mojo::PendingRemote<dialog::mojom::BaseDialogPage> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  mojo::Receiver<dialog::mojom::BaseDialogPage> receiver_{this};

  MOCK_METHOD(void, NavigateToNextStep, (notice::mojom::PrivacySandboxNotice));
};

class PrivacySandboxBaseDialogHandlerTest : public testing::Test {
 public:
  PrivacySandboxBaseDialogHandlerTest() = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockBaseDialogPage mock_page_;
  MockBaseDialogUIDelegate mock_delegate_;
  MockDesktopViewManager view_manager_;
  BaseDialogHandler handler_{mojo::NullReceiver(),
                             mock_page_.BindAndGetRemote(), &view_manager_,
                             &mock_delegate_};
};

TEST_F(PrivacySandboxBaseDialogHandlerTest, ShowDialog) {
  EXPECT_CALL(mock_delegate_, ShowNativeView()).Times(1);
  handler_.ShowDialog();
}

TEST_F(PrivacySandboxBaseDialogHandlerTest, EventOccurred) {
  EXPECT_CALL(view_manager_, OnEventOccurred(kTestNotice, kTestEvent));
  handler_.EventOccurred(kTestNotice, kTestEvent);
}

TEST_F(PrivacySandboxBaseDialogHandlerTest, ResizeDialog) {
  const int kTestHeight = 500;
  const int kTestHeight2 = 400;

  EXPECT_CALL(mock_delegate_, ResizeNativeView(kTestHeight)).Times(1);
  handler_.ResizeDialog(kTestHeight);
  // Crashes if ResizeDialog is called twice.
  EXPECT_DEATH_IF_SUPPORTED(handler_.ResizeDialog(kTestHeight2), "");
}

class PrivacySandboxBaseDialogNavigateTest
    : public PrivacySandboxBaseDialogHandlerTest,
      public testing::WithParamInterface<std::optional<PrivacySandboxNotice>> {
};

TEST_P(PrivacySandboxBaseDialogNavigateTest,
       MaybeNavigateToNextStepWithValidId) {
  std::optional<PrivacySandboxNotice> notice = GetParam();
  if (!notice) {
    EXPECT_CALL(mock_delegate_, CloseNativeView()).Times(1);
  } else {
    EXPECT_CALL(mock_delegate_, SetPrivacySandboxNotice(notice.value()))
        .Times(1);
    EXPECT_CALL(mock_page_, NavigateToNextStep(notice.value())).Times(1);
  }
  handler_.MaybeNavigateToNextStep(notice);
  mock_page_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_page_);
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxBaseDialogNavigateTest,
    PrivacySandboxBaseDialogNavigateTest,
    testing::Values(std::nullopt,
                    PrivacySandboxNotice::kTopicsConsentNotice,
                    PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
                    PrivacySandboxNotice::kThreeAdsApisNotice,
                    PrivacySandboxNotice::kMeasurementNotice));

class PrivacySandboxBaseDialogHandlerNullDelegateTest : public testing::Test {
 public:
  PrivacySandboxBaseDialogHandlerNullDelegateTest() = default;

 protected:
  MockDesktopViewManager view_manager_;
  BaseDialogHandler handler_{mojo::NullReceiver(), mojo::NullRemote(),
                             &view_manager_, nullptr};
};

TEST_F(PrivacySandboxBaseDialogHandlerNullDelegateTest, ShowDialog) {
  EXPECT_NO_FATAL_FAILURE(handler_.ShowDialog());
}

TEST_F(PrivacySandboxBaseDialogHandlerNullDelegateTest, ResizeDialog) {
  const int kTestHeight = 500;
  EXPECT_NO_FATAL_FAILURE(handler_.ResizeDialog(kTestHeight));
}

TEST_F(PrivacySandboxBaseDialogHandlerNullDelegateTest, EventOccurred) {
  EXPECT_CALL(view_manager_,
              OnEventOccurred(PrivacySandboxNotice::kTopicsConsentNotice,
                              PrivacySandboxNoticeEvent::kOptIn));
  handler_.EventOccurred(PrivacySandboxNotice::kTopicsConsentNotice,
                         PrivacySandboxNoticeEvent::kOptIn);
}

TEST_F(PrivacySandboxBaseDialogHandlerNullDelegateTest,
       MaybeNavigateToNextStep) {
  EXPECT_NO_FATAL_FAILURE(handler_.MaybeNavigateToNextStep(
      PrivacySandboxNotice::kTopicsConsentNotice));
}

}  // namespace
}  // namespace privacy_sandbox
