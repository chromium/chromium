// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::Id>>
    kEmptySelectedMap;

const std::u16string kBatchUploadTitle = u"Save data to account";

class BatchUploadDataProviderFake : public BatchUploadDataProvider {
 public:
  explicit BatchUploadDataProviderFake(BatchUploadDataType type)
      : BatchUploadDataProvider(type) {}

  void SetHasLocalData(bool has_local_data) {
    has_local_data_ = has_local_data;
  }

  bool HasLocalData() const override { return has_local_data_; }

  BatchUploadDataContainer GetLocalData() const override {
    BatchUploadDataContainer container(/*section_name_id=*/123,
                                       /*dialog_subtitle_id=*/456);
    if (has_local_data_) {
      // Add an arbitrary item.
      container.items.push_back({.id = BatchUploadDataItemModel::Id(123),
                                 .title = "data_title",
                                 .subtitle = "data_subtitle"});
    }
    return container;
  }

  bool MoveToAccountStorage(const std::vector<BatchUploadDataItemModel::Id>&
                                item_ids_to_move) override {
    return true;
  }

 private:
  bool has_local_data_ = false;
};

}  // namespace

class BatchUploadDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  BatchUploadDialogView* CreateBatchUploadDialogView(
      Profile* profile,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      SelectedDataTypeItemsCallback complete_callback) {
    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIBatchUploadURL)};
    observer.StartWatchingNewWebContents();

    BatchUploadDialogView* dialog_view =
        BatchUploadDialogView::CreateBatchUploadDialogView(
            *browser(), data_providers_list, std::move(complete_callback));

    observer.Wait();

    return dialog_view;
  }

 private:
  // Needed to make sure the mojo binders are set.
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kBatchUploadDesktop};
};

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithCloseAction) {
  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords);
  fake_provider.SetHasLocalData(true);
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), {&fake_provider}, mock_callback.Get());
  EXPECT_EQ(dialog_view->GetWindowTitle(), kBatchUploadTitle);

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  dialog_view->OnDialogSelectionMade({});
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithDestroyed) {
  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  {
    BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords);
    fake_provider.SetHasLocalData(true);
    BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
        browser()->profile(), {&fake_provider}, mock_callback.Get());
    EXPECT_EQ(dialog_view->GetWindowTitle(), kBatchUploadTitle);

    // Simulate the widget closing without user action.
    views::Widget* widget = dialog_view->GetWidget();
    ASSERT_TRUE(widget);
    widget->Close();
  }
}
