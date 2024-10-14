// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_test.h"

namespace {

constexpr base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::DataId>>
    kEmptySelectedMap;

class BatchUploadDataProviderFake : public BatchUploadDataProvider {
 public:
  explicit BatchUploadDataProviderFake(BatchUploadDataType type, int item_count)
      : BatchUploadDataProvider(type), item_count_(item_count) {}

  bool HasLocalData() const override { return item_count_ > 0; }

  BatchUploadDataContainer GetLocalData() const override {
    // IDs used here are arbitrary and should not be checked.
    BatchUploadDataContainer container(
        GetDataType(),
        /*section_name_id=*/IDS_BATCH_UPLOAD_SECTION_TITLE_PASSWORDS);
    // Add arbitrary items.
    for (int i = 0; i < item_count_; ++i) {
      BatchUploadDataItemModel item;
      std::string index_string = base::ToString(i);
      item.id = BatchUploadDataItemModel::DataId(index_string);
      item.title = "data_title_" + index_string;
      item.subtitle = "data_subtitle_" + index_string;
      container.items.push_back(std::move(item));
    }
    return container;
  }

  bool MoveToAccountStorage(const std::vector<BatchUploadDataItemModel::DataId>&
                                item_ids_to_move) override {
    return true;
  }

  std::vector<BatchUploadDataItemModel::DataId> GetItemIds() {
    std::vector<BatchUploadDataItemModel::DataId> item_ids;
    for (int i = 0; i < item_count_; ++i) {
      item_ids.push_back(BatchUploadDataItemModel::DataId(base::ToString(i)));
    }
    return item_ids;
  }

 private:
  int item_count_ = 0;
};

// Unable to use `content::SimulateKeyPress()` helper function since it sets
// `event.skip_if_unhandled` to true which stops the propagation of the event to
// the delegate web view.
void SimulateEscapeKeyPress(content::WebContents* web_content) {
  // Create the escape key press event.
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  event.dom_key = ui::DomKey::ESCAPE;
  event.dom_code = static_cast<int>(ui::DomCode::ESCAPE);

  // Send the event to the Web Contents.
  web_content->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardKeyboardEvent(event);
}

}  // namespace

class BatchUploadDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  BatchUploadDialogView* CreateBatchUploadDialogView(
      Profile* profile,
      std::vector<BatchUploadDataContainer> data_containers,
      SelectedDataTypeItemsCallback complete_callback) {
    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIBatchUploadURL)};
    observer.StartWatchingNewWebContents();

    BatchUploadDialogView* dialog_view =
        BatchUploadDialogView::CreateBatchUploadDialogView(
            *browser(), std::move(data_containers),
            std::move(complete_callback));

    observer.Wait();

    views::test::WidgetVisibleWaiter(dialog_view->GetWidget()).Wait();

    return dialog_view;
  }

  void SigninWithFullInfo() {
    signin::IdentityManager* identity_manager = GetIdentityManager();
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@gmail.com", signin::ConsentLevel::kSignin);
    ASSERT_FALSE(account_info.IsEmpty());

    account_info.full_name = "Joe Testing";
    account_info.given_name = "Joe";
    account_info.picture_url = "SOME_FAKE_URL";
    account_info.hosted_domain = kNoHostedDomainFound;
    account_info.locale = "en";
    ASSERT_TRUE(account_info.IsValid());
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  void Signout() { signin::ClearPrimaryAccount(GetIdentityManager()); }

  void TriggerSigninPending() {
    signin::SetInvalidRefreshTokenForPrimaryAccount(GetIdentityManager());
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  base::HistogramTester histogram_tester_;

  // Needed to make sure the mojo binders are set.
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kBatchUploadDesktop};
};

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithCancelAction) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords, 1);
  std::vector<BatchUploadDataContainer> containers;
  containers.push_back(fake_provider.GetLocalData());
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), std::move(containers), mock_callback.Get());

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);

  dialog_view->OnDialogSelectionMade({});
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Signin.BatchUpload.Opened", 1},
      {"Signin.BatchUpload.DataTypeAvailable", 1},
      {"Signin.BatchUpload.DialogCloseReason", 1}};
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.Opened", true, 1);
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.DataTypeAvailable",
                                        fake_provider.GetDataType(), 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kCancelClicked, 1);
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithDestroyed) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  BatchUploadDataType input_type = BatchUploadDataType::kPasswords;
  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  {
    BatchUploadDataProviderFake fake_provider(input_type, 1);
    std::vector<BatchUploadDataContainer> containers;
    containers.push_back(fake_provider.GetLocalData());
    BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
        browser()->profile(), std::move(containers), mock_callback.Get());

    // Simulate the widget closing without user action.
    views::Widget* widget = dialog_view->GetWidget();
    ASSERT_TRUE(widget);
    widget->Close();
    views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();
  }

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Signin.BatchUpload.Opened", 1},
      {"Signin.BatchUpload.DataTypeAvailable", 1},
      {"Signin.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.Opened", true, 1);
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.DataTypeAvailable",
                                        input_type, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kWindowClosed, 1);
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewDismiss) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;
  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords, 1);
  std::vector<BatchUploadDataContainer> containers;
  containers.push_back(fake_provider.GetLocalData());
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), std::move(containers), mock_callback.Get());

  // Pressing the escape key should dismiss the dialog and return empty result.
  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  views::test::WidgetDestroyedWaiter destroyed_waiter(dialog_view->GetWidget());
  SimulateEscapeKeyPress(dialog_view->GetWebViewForTesting()->GetWebContents());
  destroyed_waiter.Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Signin.BatchUpload.Opened", 1},
      {"Signin.BatchUpload.DataTypeAvailable", 1},
      {"Signin.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.Opened", true, 1);
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.DataTypeAvailable",
                                        fake_provider.GetDataType(), 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kDismissed, 1);
}

// Fails on Mac only.  http://crbug.com/372194892
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenBatchUploadDialogViewClosesOnSignout \
  DISABLED_OpenBatchUploadDialogViewClosesOnSignout
#else
#define MAYBE_OpenBatchUploadDialogViewClosesOnSignout \
  OpenBatchUploadDialogViewClosesOnSignout
#endif
IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       MAYBE_OpenBatchUploadDialogViewClosesOnSignout) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);

  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords, 1);
  std::vector<BatchUploadDataContainer> containers;
  containers.push_back(fake_provider.GetLocalData());
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), std::move(containers), mock_callback.Get());
  ASSERT_TRUE(dialog_view->GetWidget()->IsVisible());

  // Signing out should close the dialog.
  Signout();
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Signin.BatchUpload.Opened", 1},
      {"Signin.BatchUpload.DataTypeAvailable", 1},
      {"Signin.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.Opened", true, 1);
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.DataTypeAvailable",
                                        fake_provider.GetDataType(), 1);
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.DialogCloseReason",
                                        BatchUploadDialogCloseReason::kSignout,
                                        1);
}

// Fails on Mac only.  http://crbug.com/372194892
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenBatchUploadDialogViewClosesOnSigninPending \
  DISABLED_OpenBatchUploadDialogViewClosesOnSigninPending
#else
#define MAYBE_OpenBatchUploadDialogViewClosesOnSigninPending \
  OpenBatchUploadDialogViewClosesOnSigninPending
#endif
IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       MAYBE_OpenBatchUploadDialogViewClosesOnSigninPending) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);

  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords, 1);
  std::vector<BatchUploadDataContainer> containers;
  containers.push_back(fake_provider.GetLocalData());
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), std::move(containers), mock_callback.Get());
  ASSERT_TRUE(dialog_view->GetWidget()->IsVisible());

  // Signing out should close the dialog.
  TriggerSigninPending();
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Signin.BatchUpload.Opened", 1},
      {"Signin.BatchUpload.DataTypeAvailable", 1},
      {"Signin.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.Opened", true, 1);
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.DataTypeAvailable",
                                        fake_provider.GetDataType(), 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kSiginPending, 1);
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithSaveActionAllItems) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords, 1);
  BatchUploadDataProviderFake fake_provider2(BatchUploadDataType::kAddresses,
                                             2);
  std::vector<BatchUploadDataContainer> containers;
  containers.push_back(fake_provider.GetLocalData());
  containers.push_back(fake_provider2.GetLocalData());
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), std::move(containers), mock_callback.Get());

  base::flat_map<BatchUploadDataType,
                 std::vector<BatchUploadDataItemModel::DataId>>
      result;
  result.insert_or_assign(fake_provider.GetDataType(),
                          fake_provider.GetItemIds());
  result.insert_or_assign(fake_provider2.GetDataType(),
                          fake_provider2.GetItemIds());
  EXPECT_CALL(mock_callback, Run(result)).Times(1);
  dialog_view->OnDialogSelectionMade(result);
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Signin.BatchUpload.Opened", 1},
      {"Signin.BatchUpload.DataTypeAvailable", 2},
      {"Signin.BatchUpload.DataTypeSelected", 2},
      {"Signin.BatchUpload.DataTypeSelectedItemPercentage", 2},
      {"Signin.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.Opened", true, 1);
  histogram_tester().ExpectBucketCount("Signin.BatchUpload.DataTypeAvailable",
                                       fake_provider.GetDataType(), 1);
  histogram_tester().ExpectBucketCount("Signin.BatchUpload.DataTypeAvailable",
                                       fake_provider2.GetDataType(), 1);
  histogram_tester().ExpectBucketCount("Signin.BatchUpload.DataTypeSelected",
                                       fake_provider.GetDataType(), 1);
  histogram_tester().ExpectBucketCount("Signin.BatchUpload.DataTypeSelected",
                                       fake_provider2.GetDataType(), 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.BatchUpload.DataTypeSelectedItemPercentage", 100, 2);
  histogram_tester().ExpectUniqueSample(
      "Signin.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kSaveClicked, 1);
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithSaveActionSomeItems) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords, 1);
  BatchUploadDataProviderFake fake_provider2(BatchUploadDataType::kAddresses,
                                             2);
  std::vector<BatchUploadDataContainer> containers;
  containers.push_back(fake_provider.GetLocalData());
  containers.push_back(fake_provider2.GetLocalData());
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), std::move(containers), mock_callback.Get());

  base::flat_map<BatchUploadDataType,
                 std::vector<BatchUploadDataItemModel::DataId>>
      result;
  std::vector<BatchUploadDataItemModel::DataId> empty;
  result.insert_or_assign(fake_provider.GetDataType(), empty);
  // Remove one element of the two.
  auto partial_selection_ids_container_2 = fake_provider2.GetItemIds();
  partial_selection_ids_container_2.pop_back();
  ASSERT_GE(partial_selection_ids_container_2.size(), 1u);
  result.insert_or_assign(fake_provider2.GetDataType(),
                          partial_selection_ids_container_2);
  EXPECT_CALL(mock_callback, Run(result)).Times(1);
  // Result is of the form {{}, {"0"}}.
  dialog_view->OnDialogSelectionMade(result);
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Signin.BatchUpload.Opened", 1},
      {"Signin.BatchUpload.DataTypeAvailable", 2},
      {"Signin.BatchUpload.DataTypeSelected", 1},
      {"Signin.BatchUpload.DataTypeSelectedItemPercentage", 1},
      {"Signin.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Signin.BatchUpload.Opened", true, 1);
  histogram_tester().ExpectBucketCount("Signin.BatchUpload.DataTypeAvailable",
                                       fake_provider.GetDataType(), 1);
  histogram_tester().ExpectBucketCount("Signin.BatchUpload.DataTypeAvailable",
                                       fake_provider2.GetDataType(), 1);
  histogram_tester().ExpectBucketCount("Signin.BatchUpload.DataTypeSelected",
                                       fake_provider.GetDataType(), 0);
  histogram_tester().ExpectBucketCount("Signin.BatchUpload.DataTypeSelected",
                                       fake_provider2.GetDataType(), 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.BatchUpload.DataTypeSelectedItemPercentage", 50, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kSaveClicked, 1);
}
