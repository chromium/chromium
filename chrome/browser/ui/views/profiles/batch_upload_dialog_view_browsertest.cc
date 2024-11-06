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
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
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

const std::map<syncer::DataType,
               std::vector<syncer::LocalDataItemModel::DataId>>
    kEmptySelectedMap;

syncer::LocalDataDescription GetFakeLocalData(syncer::DataType type,
                                              int item_count) {
  // IDs used here are arbitrary and should not be checked.
  syncer::LocalDataDescription description;
  description.type = type;
  // Add arbitrary items.
  for (int i = 0; i < item_count; ++i) {
    syncer::LocalDataItemModel item;
    std::string index_string = base::ToString(i);
    item.id = syncer::LocalDataItemModel::DataId(index_string);
    item.title = "data_title_" + index_string;
    item.subtitle = "data_subtitle_" + index_string;
    description.local_data_models.push_back(std::move(item));
  }
  return description;
}

std::vector<syncer::LocalDataItemModel::DataId> GetItemIds(int item_count) {
  std::vector<syncer::LocalDataItemModel::DataId> item_ids;
  for (int i = 0; i < item_count; ++i) {
    item_ids.emplace_back(base::ToString(i));
  }
  return item_ids;
}

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
      std::vector<syncer::LocalDataDescription> local_data_description_list,
      BatchUploadService::EntryPoint entry_point,
      BatchUploadSelectedDataTypeItemsCallback complete_callback) {
    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIBatchUploadURL)};
    observer.StartWatchingNewWebContents();

    BatchUploadDialogView* dialog_view =
        BatchUploadDialogView::CreateBatchUploadDialogView(
            *browser(), std::move(local_data_description_list), entry_point,
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

  base::MockCallback<BatchUploadSelectedDataTypeItemsCallback> mock_callback;

  std::vector<syncer::LocalDataDescription> descriptions;
  syncer::DataType type = syncer::DataType::PASSWORDS;
  descriptions.push_back(GetFakeLocalData(type, 1));
  BatchUploadService::EntryPoint entry_point =
      BatchUploadService::EntryPoint::kPasswordManagerSettings;
  BatchUploadDialogView* dialog_view =
      CreateBatchUploadDialogView(browser()->profile(), std::move(descriptions),
                                  entry_point, mock_callback.Get());

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);

  dialog_view->OnDialogSelectionMade({});
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Sync.BatchUpload.Opened", 1},
      {"Sync.BatchUpload.DataTypeAvailable", 1},
      {"Sync.BatchUpload.DialogCloseReason", 1}};
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Sync.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.Opened", entry_point,
                                        1);
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.DataTypeAvailable",
                                        DataTypeHistogramValue(type), 1);
  histogram_tester().ExpectUniqueSample(
      "Sync.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kCancelClicked, 1);
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithDestroyed) {
  SigninWithFullInfo();

  base::MockCallback<BatchUploadSelectedDataTypeItemsCallback> mock_callback;

  syncer::DataType input_type = syncer::DataType::PASSWORDS;
  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  BatchUploadService::EntryPoint entry_point =
      BatchUploadService::EntryPoint::kPasswordManagerSettings;
  {
    std::vector<syncer::LocalDataDescription> descriptions;
    descriptions.push_back(GetFakeLocalData(input_type, 1));
    BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
        browser()->profile(), std::move(descriptions), entry_point,
        mock_callback.Get());

    // Simulate the widget closing without user action.
    views::Widget* widget = dialog_view->GetWidget();
    ASSERT_TRUE(widget);
    widget->Close();
    views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();
  }

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Sync.BatchUpload.Opened", 1},
      {"Sync.BatchUpload.DataTypeAvailable", 1},
      {"Sync.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Sync.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.Opened", entry_point,
                                        1);
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.DataTypeAvailable",
                                        DataTypeHistogramValue(input_type), 1);
  histogram_tester().ExpectUniqueSample(
      "Sync.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kWindowClosed, 1);
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewDismiss) {
  SigninWithFullInfo();

  base::MockCallback<BatchUploadSelectedDataTypeItemsCallback> mock_callback;
  std::vector<syncer::LocalDataDescription> descriptions;
  syncer::DataType type = syncer::DataType::PASSWORDS;
  descriptions.push_back(GetFakeLocalData(type, 1));
  BatchUploadService::EntryPoint entry_point =
      BatchUploadService::EntryPoint::kPasswordPromoCard;
  BatchUploadDialogView* dialog_view =
      CreateBatchUploadDialogView(browser()->profile(), std::move(descriptions),
                                  entry_point, mock_callback.Get());

  // Pressing the escape key should dismiss the dialog and return empty result.
  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  views::test::WidgetDestroyedWaiter destroyed_waiter(dialog_view->GetWidget());
  SimulateEscapeKeyPress(dialog_view->GetWebViewForTesting()->GetWebContents());
  destroyed_waiter.Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Sync.BatchUpload.Opened", 1},
      {"Sync.BatchUpload.DataTypeAvailable", 1},
      {"Sync.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Sync.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.Opened", entry_point,
                                        1);
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.DataTypeAvailable",
                                        DataTypeHistogramValue(type), 1);
  histogram_tester().ExpectUniqueSample(
      "Sync.BatchUpload.DialogCloseReason",
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

  base::MockCallback<BatchUploadSelectedDataTypeItemsCallback> mock_callback;

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);

  std::vector<syncer::LocalDataDescription> descriptions;
  syncer::DataType type = syncer::DataType::PASSWORDS;
  descriptions.push_back(GetFakeLocalData(type, 1));
  BatchUploadService::EntryPoint entry_point =
      BatchUploadService::EntryPoint::kPasswordPromoCard;
  BatchUploadDialogView* dialog_view =
      CreateBatchUploadDialogView(browser()->profile(), std::move(descriptions),
                                  entry_point, mock_callback.Get());
  ASSERT_TRUE(dialog_view->GetWidget()->IsVisible());

  // Signing out should close the dialog.
  Signout();
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Sync.BatchUpload.Opened", 1},
      {"Sync.BatchUpload.DataTypeAvailable", 1},
      {"Sync.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Sync.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.Opened", entry_point,
                                        1);
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.DataTypeAvailable",
                                        DataTypeHistogramValue(type), 1);
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.DialogCloseReason",
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

  base::MockCallback<BatchUploadSelectedDataTypeItemsCallback> mock_callback;

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);

  std::vector<syncer::LocalDataDescription> descriptions;
  syncer::DataType type = syncer::DataType::PASSWORDS;
  descriptions.push_back(GetFakeLocalData(type, 1));
  BatchUploadService::EntryPoint entry_point =
      BatchUploadService::EntryPoint::kPasswordPromoCard;
  BatchUploadDialogView* dialog_view =
      CreateBatchUploadDialogView(browser()->profile(), std::move(descriptions),
                                  entry_point, mock_callback.Get());
  ASSERT_TRUE(dialog_view->GetWidget()->IsVisible());

  // Signing out should close the dialog.
  TriggerSigninPending();
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Sync.BatchUpload.Opened", 1},
      {"Sync.BatchUpload.DataTypeAvailable", 1},
      {"Sync.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Sync.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.Opened", entry_point,
                                        1);
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.DataTypeAvailable",
                                        DataTypeHistogramValue(type), 1);
  histogram_tester().ExpectUniqueSample(
      "Sync.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kSiginPending, 1);
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithSaveActionAllItems) {
  SigninWithFullInfo();

  base::MockCallback<BatchUploadSelectedDataTypeItemsCallback> mock_callback;

  std::vector<syncer::LocalDataDescription> descriptions;
  syncer::DataType type1 = syncer::DataType::PASSWORDS;
  int count1 = 1;
  descriptions.push_back(GetFakeLocalData(type1, count1));
  syncer::DataType type2 = syncer::DataType::CONTACT_INFO;
  int count2 = 2;
  descriptions.push_back(GetFakeLocalData(type2, count2));
  BatchUploadService::EntryPoint entry_point =
      BatchUploadService::EntryPoint::kPasswordPromoCard;
  BatchUploadDialogView* dialog_view =
      CreateBatchUploadDialogView(browser()->profile(), std::move(descriptions),
                                  entry_point, mock_callback.Get());

  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      result;
  result.insert_or_assign(type1, GetItemIds(count1));
  result.insert_or_assign(type2, GetItemIds(count2));
  EXPECT_CALL(mock_callback, Run(result)).Times(1);
  dialog_view->OnDialogSelectionMade(result);
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Sync.BatchUpload.Opened", 1},
      {"Sync.BatchUpload.DataTypeAvailable", 2},
      {"Sync.BatchUpload.DataTypeSelected", 2},
      {"Sync.BatchUpload.DataTypeSelectedItemPercentage", 2},
      {"Sync.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Sync.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.Opened", entry_point,
                                        1);
  histogram_tester().ExpectBucketCount("Sync.BatchUpload.DataTypeAvailable",
                                       DataTypeHistogramValue(type1), 1);
  histogram_tester().ExpectBucketCount("Sync.BatchUpload.DataTypeAvailable",
                                       DataTypeHistogramValue(type2), 1);
  histogram_tester().ExpectBucketCount("Sync.BatchUpload.DataTypeSelected",
                                       DataTypeHistogramValue(type1), 1);
  histogram_tester().ExpectBucketCount("Sync.BatchUpload.DataTypeSelected",
                                       DataTypeHistogramValue(type2), 1);
  histogram_tester().ExpectUniqueSample(
      "Sync.BatchUpload.DataTypeSelectedItemPercentage", 100, 2);
  histogram_tester().ExpectUniqueSample(
      "Sync.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kSaveClicked, 1);
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithSaveActionSomeItems) {
  SigninWithFullInfo();

  base::MockCallback<BatchUploadSelectedDataTypeItemsCallback> mock_callback;

  std::vector<syncer::LocalDataDescription> descriptions;
  syncer::DataType type1 = syncer::DataType::PASSWORDS;
  descriptions.push_back(GetFakeLocalData(type1, 1));
  syncer::DataType type2 = syncer::DataType::CONTACT_INFO;
  int count2 = 2;
  descriptions.push_back(GetFakeLocalData(type2, count2));
  BatchUploadService::EntryPoint entry_point =
      BatchUploadService::EntryPoint::kPasswordPromoCard;
  BatchUploadDialogView* dialog_view =
      CreateBatchUploadDialogView(browser()->profile(), std::move(descriptions),
                                  entry_point, mock_callback.Get());

  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      result;
  std::vector<syncer::LocalDataItemModel::DataId> empty;
  result.insert_or_assign(type1, empty);
  // Remove one element of the two.
  auto partial_selection_ids_descriptions_2 = GetItemIds(count2);
  partial_selection_ids_descriptions_2.pop_back();
  ASSERT_GE(partial_selection_ids_descriptions_2.size(), 1u);
  result.insert_or_assign(type2, partial_selection_ids_descriptions_2);
  EXPECT_CALL(mock_callback, Run(result)).Times(1);
  // Result is of the form {{}, {"0"}}.
  dialog_view->OnDialogSelectionMade(result);
  views::test::WidgetDestroyedWaiter(dialog_view->GetWidget()).Wait();

  base::HistogramTester::CountsMap expected_histograms_count = {
      {"Sync.BatchUpload.Opened", 1},
      {"Sync.BatchUpload.DataTypeAvailable", 2},
      {"Sync.BatchUpload.DataTypeSelected", 1},
      {"Sync.BatchUpload.DataTypeSelectedItemPercentage", 1},
      {"Sync.BatchUpload.DialogCloseReason", 1},
  };
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Sync.BatchUpload."),
              testing::ContainerEq(expected_histograms_count));
  histogram_tester().ExpectUniqueSample("Sync.BatchUpload.Opened", entry_point,
                                        1);
  histogram_tester().ExpectBucketCount("Sync.BatchUpload.DataTypeAvailable",
                                       DataTypeHistogramValue(type1), 1);
  histogram_tester().ExpectBucketCount("Sync.BatchUpload.DataTypeAvailable",
                                       DataTypeHistogramValue(type2), 1);
  histogram_tester().ExpectBucketCount("Sync.BatchUpload.DataTypeSelected",
                                       DataTypeHistogramValue(type1), 0);
  histogram_tester().ExpectBucketCount("Sync.BatchUpload.DataTypeSelected",
                                       DataTypeHistogramValue(type2), 1);
  histogram_tester().ExpectUniqueSample(
      "Sync.BatchUpload.DataTypeSelectedItemPercentage", 50, 1);
  histogram_tester().ExpectUniqueSample(
      "Sync.BatchUpload.DialogCloseReason",
      BatchUploadDialogCloseReason::kSaveClicked, 1);
}
