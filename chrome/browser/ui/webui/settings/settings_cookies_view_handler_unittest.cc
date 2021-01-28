// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/browsing_data/content/mock_cookie_helper.h"
#include "components/browsing_data/content/mock_local_storage_helper.h"
#include "content/public/test/test_web_ui.h"

namespace {

constexpr char kCallbackId[] = "test-callback-id";
constexpr char kTestOrigin1[] = "https://a-example.com";
constexpr char kTestOrigin2[] = "https://b-example.com";
constexpr char kTestHost1[] = "a-example.com";
constexpr char kTestHost2[] = "b-example.com";
constexpr char kTestCookie1[] = "A=1";
constexpr char kTestCookie2[] = "B=1";

}  // namespace

namespace settings {

class CookiesViewHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    handler_ = std::make_unique<CookiesViewHandler>();
    handler_->set_web_ui(web_ui());
    web_ui_->ClearTrackedCalls();
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetupTreeModelForTesting() {
    mock_browsing_data_cookie_helper =
        base::MakeRefCounted<browsing_data::MockCookieHelper>(profile());
    mock_browsing_data_local_storage_helper =
        base::MakeRefCounted<browsing_data::MockLocalStorageHelper>(profile());

    auto container = std::make_unique<LocalDataContainer>(
        mock_browsing_data_cookie_helper,
        /*database_helper=*/nullptr, mock_browsing_data_local_storage_helper,
        /*session_storage_helper=*/nullptr,
        /*appcache_helper=*/nullptr,
        /*indexed_db_helper=*/nullptr,
        /*file_system_helper=*/nullptr,
        /*quota_helper=*/nullptr,
        /*service_worker_helper=*/nullptr,
        /*data_shared_worker_helper=*/nullptr,
        /*cache_storage_helper=*/nullptr,
        /*media_license_helper=*/nullptr);
    auto mock_cookies_tree_model = std::make_unique<CookiesTreeModel>(
        std::move(container), profile()->GetExtensionSpecialStoragePolicy());

    mock_browsing_data_local_storage_helper->AddLocalStorageForOrigin(
        url::Origin::Create(GURL(kTestOrigin1)), 2);
    mock_browsing_data_local_storage_helper->AddLocalStorageForOrigin(
        url::Origin::Create(GURL(kTestOrigin2)), 3);

    mock_browsing_data_cookie_helper->AddCookieSamples(GURL(kTestOrigin1),
                                                       kTestCookie1);
    mock_browsing_data_cookie_helper->AddCookieSamples(GURL(kTestOrigin2),
                                                       kTestCookie2);

    handler()->SetCookiesTreeModelForTesting(
        std::move(mock_cookies_tree_model));
  }

  void NotifyTreeModel() {
    mock_browsing_data_local_storage_helper->Notify();
    mock_browsing_data_cookie_helper->Notify();
  }

  void SetupHandlerWithTreeModel() {
    SetupTreeModelForTesting();
    base::ListValue reload_args;
    reload_args.AppendString(kCallbackId);
    handler()->HandleReloadCookies(&reload_args);

    // The handler will post a task to recreate the tree model.
    task_environment()->RunUntilIdle();
    NotifyTreeModel();

    // After batch end, the handler will have posted a task to complete
    // the callback.
    task_environment()->RunUntilIdle();
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    web_ui_->ClearTrackedCalls();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  CookiesViewHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<CookiesViewHandler> handler_;

  // Ref pointers to storage helpers used in the tree model used for testing.
  // Retained to allow control over batch update completion.
  scoped_refptr<browsing_data::MockLocalStorageHelper>
      mock_browsing_data_local_storage_helper;
  scoped_refptr<browsing_data::MockCookieHelper>
      mock_browsing_data_cookie_helper;
};

TEST_F(CookiesViewHandlerTest, SingleRequestDuringBatch) {
  // Ensure that multiple requests do not run while a tree model batch process
  // is running.
  SetupTreeModelForTesting();

  constexpr char kReloadCallbackID[] = "reload-cookies-callback";
  constexpr char kGetDisplaylistCallbackID[] = "get-display-list-callback";

  base::ListValue reload_args;
  reload_args.AppendString(kReloadCallbackID);
  handler()->HandleReloadCookies(&reload_args);
  task_environment()->RunUntilIdle();

  // At the point the handler will have recreated the model (using the provided
  // test model) and will be awaiting batch end. Performing another request
  // should result in it not being satisfied, and instead being queued.
  base::ListValue get_display_list_args;
  get_display_list_args.AppendString(kGetDisplaylistCallbackID);
  get_display_list_args.AppendString("");
  handler()->HandleGetDisplayList(&get_display_list_args);
  task_environment()->RunUntilIdle();

  // Because the tree model hasn't completed the batch, no callback should
  // have been completed.
  EXPECT_EQ(0U, web_ui()->call_data().size());

  // Completing the tree model batch should result in both callbacks being
  // completed in the correct order.
  NotifyTreeModel();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(2U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& reload_response =
      *web_ui()->call_data().front();
  EXPECT_EQ("cr.webUIResponse", reload_response.function_name());
  EXPECT_EQ(kReloadCallbackID, reload_response.arg1()->GetString());
  ASSERT_TRUE(reload_response.arg2()->GetBool());

  const content::TestWebUI::CallData& get_display_list_response =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", get_display_list_response.function_name());
  EXPECT_EQ(kGetDisplaylistCallbackID,
            get_display_list_response.arg1()->GetString());
  ASSERT_TRUE(get_display_list_response.arg2()->GetBool());
  base::Value::ConstListView local_data_list =
      get_display_list_response.arg3()->GetList();
  ASSERT_EQ(2U, local_data_list.size());
  EXPECT_EQ(kTestHost1, local_data_list[0].FindKey("site")->GetString());
  EXPECT_EQ(kTestHost2, local_data_list[1].FindKey("site")->GetString());
}

TEST_F(CookiesViewHandlerTest, NoStarvation) {
  SetupHandlerWithTreeModel();

  // Confirm that the request queue does not starve during various combinations
  // of requests. This is achieved by queueing numerous permutations of requests
  // which have different interaction properties with the tree model.
  std::string current_filter = kTestHost1;
  auto get_display_list_new_filter =
      base::BindLambdaForTesting([&](std::string callback_id) {
        base::ListValue args;
        args.AppendString(callback_id);
        current_filter = current_filter == kTestHost1 ? "" : kTestHost1;
        args.AppendString(kTestHost1);
        handler()->HandleGetDisplayList(&args);
      });
  auto get_display_list_same_filter =
      base::BindLambdaForTesting([&](std::string callback_id) {
        base::ListValue args;
        args.AppendString(callback_id);
        args.AppendString(current_filter);
        handler()->HandleGetDisplayList(&args);
      });
  auto get_cookie_details =
      base::BindLambdaForTesting([&](std::string callback_id) {
        base::ListValue args;
        args.AppendString(callback_id);
        args.AppendString(kTestHost1);
        handler()->HandleGetCookieDetails(&args);
      });
  auto reload_cookies =
      base::BindLambdaForTesting([&](std::string callback_id) {
        base::ListValue args;
        args.AppendString(callback_id);
        handler()->HandleReloadCookies(&args);
      });
  auto remove_third_party =
      base::BindLambdaForTesting([&](std::string callback_id) {
        base::ListValue args;
        args.AppendString(callback_id);
        handler()->HandleRemoveThirdParty(&args);
      });
  // Include a dummy request which allows the request queue to be cleared. This
  // ensures that requests may be queued up both during, and outside of, batch
  // updates.
  auto process_pending_requests = base::BindLambdaForTesting(
      [&](std::string callback_id) { task_environment()->RunUntilIdle(); });

  // For completeness, test every permutation of these calls.
  std::vector<std::pair<int, base::RepeatingCallback<void(std::string)>>>
      request_functions = {
          {1, get_display_list_new_filter}, {2, get_display_list_same_filter},
          {3, get_cookie_details},          {4, reload_cookies},
          {5, remove_third_party},          {6, process_pending_requests},
      };
  auto request_function_ordering =
      [](const std::pair<int, base::RepeatingCallback<void(std::string)>>& left,
         std::pair<int, base::RepeatingCallback<void(std::string)>>& right) {
        return left.first < right.first;
      };
  std::sort(request_functions.begin(), request_functions.end(),
            request_function_ordering);

  size_t expected_response_count = 0;
  do {
    for (const auto& function : request_functions) {
      // Provide a unique callback ID for each request.
      function.second.Run(
          std::string(kCallbackId)
              .append(base::NumberToString(expected_response_count)));

      if (function.second != process_pending_requests)
        expected_response_count++;
    }
  } while (std::next_permutation(request_functions.begin(),
                                 request_functions.end(),
                                 request_function_ordering));

  task_environment()->RunUntilIdle();

  // Ensure that callbacks have been fulfilled in the order they were queued.
  ASSERT_EQ(expected_response_count, web_ui()->call_data().size());
  for (size_t i = 0; i < expected_response_count; i++) {
    EXPECT_EQ(std::string(kCallbackId).append(base::NumberToString(i)),
              web_ui()->call_data()[i]->arg1()->GetString());
  }
}

TEST_F(CookiesViewHandlerTest, ImmediateTreeOperation) {
  // Check that a query which assumes a tree model to have been created
  // previously results in a tree being created before the request is handled.
  SetupTreeModelForTesting();

  base::ListValue args;
  args.AppendString(kCallbackId);
  args.AppendString(kTestHost1);
  handler()->HandleGetCookieDetails(&args);
  task_environment()->RunUntilIdle();

  // At this point the handler should have queued the creation of a tree and
  // be awaiting batch completion.
  NotifyTreeModel();
  task_environment()->RunUntilIdle();

  // Check that the returned information is accurate, despite not having
  // previously loaded the tree.
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());

  base::Value::ConstListView cookies_list = data.arg3()->GetList();
  ASSERT_EQ(2UL, cookies_list.size());
  EXPECT_EQ("cookie", cookies_list[0].FindKey("type")->GetString());
  EXPECT_EQ("local_storage", cookies_list[1].FindKey("type")->GetString());
}

TEST_F(CookiesViewHandlerTest, HandleGetDisplayList) {
  // Ensure that getting the display list works appropriately.
  SetupHandlerWithTreeModel();

  // Retrieve a filtered list.
  {
    base::ListValue args;
    args.AppendString(kCallbackId);
    args.AppendString(kTestHost1);

    handler()->HandleGetDisplayList(&args);
    task_environment()->RunUntilIdle();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    base::Value::ConstListView local_data_list = data.arg3()->GetList();
    ASSERT_EQ(1U, local_data_list.size());
    EXPECT_EQ(kTestHost1, local_data_list[0].FindKey("site")->GetString());
  }

  // Remove the filter and confirm the full list is returned.
  {
    base::ListValue args;
    args.AppendString(kCallbackId);
    args.AppendString("");

    handler()->HandleGetDisplayList(&args);
    task_environment()->RunUntilIdle();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    base::Value::ConstListView local_data_list = data.arg3()->GetList();
    ASSERT_EQ(2U, local_data_list.size());
    EXPECT_EQ(kTestHost1, local_data_list[0].FindKey("site")->GetString());
    EXPECT_EQ(kTestHost2, local_data_list[1].FindKey("site")->GetString());
  }
}

TEST_F(CookiesViewHandlerTest, HandleRemoveShownItems) {
  // Ensure that removing shown items only removes items appropriate for the
  // current filter.
  SetupHandlerWithTreeModel();

  // Apply a filter to the list and confirm it is returned.
  {
    base::ListValue args;
    args.AppendString(kCallbackId);
    args.AppendString(kTestHost2);
    handler()->HandleGetDisplayList(&args);
    task_environment()->RunUntilIdle();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    base::Value::ConstListView local_data_list = data.arg3()->GetList();
    ASSERT_EQ(1U, local_data_list.size());
    EXPECT_EQ(kTestHost2, local_data_list[0].FindKey("site")->GetString());
  }

  // Remove displayed items.
  {
    base::ListValue args;
    handler()->HandleRemoveShownItems(&args);
    task_environment()->RunUntilIdle();
  }

  // Remove the filter and confirm unremoved items are returned.
  {
    base::ListValue args;
    args.AppendString(kCallbackId);
    args.AppendString("");
    handler()->HandleGetDisplayList(&args);
    task_environment()->RunUntilIdle();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    base::Value::ConstListView local_data_list = data.arg3()->GetList();
    ASSERT_EQ(1U, local_data_list.size());
    EXPECT_EQ(kTestHost1, local_data_list[0].FindKey("site")->GetString());
  }
}

TEST_F(CookiesViewHandlerTest, HandleGetCookieDetails) {
  // Ensure that the cookie details are correctly returned for a site.
  SetupHandlerWithTreeModel();
  base::ListValue args;
  args.AppendString(kCallbackId);
  args.AppendString(kTestHost1);
  handler()->HandleGetCookieDetails(&args);
  task_environment()->RunUntilIdle();

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());

  base::Value::ConstListView cookies_list = data.arg3()->GetList();
  ASSERT_EQ(2UL, cookies_list.size());
  EXPECT_EQ("cookie", cookies_list[0].FindKey("type")->GetString());
  EXPECT_EQ("local_storage", cookies_list[1].FindKey("type")->GetString());
}

TEST_F(CookiesViewHandlerTest, HandleRemoveAll) {
  // Ensure that RemoveAll removes all cookies & storage.
  SetupHandlerWithTreeModel();
  {
    base::ListValue args;
    args.AppendString(kCallbackId);
    handler()->HandleRemoveAll(&args);
    task_environment()->RunUntilIdle();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    ASSERT_TRUE(data.arg2()->GetBool());
  }

  // Ensure returned display list is empty.
  {
    base::ListValue args;
    args.AppendString(kCallbackId);
    args.AppendString("");
    handler()->HandleGetDisplayList(&args);
    task_environment()->RunUntilIdle();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    base::Value::ConstListView local_data_list = data.arg3()->GetList();
    ASSERT_EQ(0U, local_data_list.size());
  }
}

TEST_F(CookiesViewHandlerTest, HandleRemoveItem) {
  // Delete an individual piece of site data. This requires first getting the
  // node path ID via the HandleGetCookieDetails function.
  SetupHandlerWithTreeModel();

  // Get the appropriate path for removal.
  std::string node_path_id;
  {
    base::ListValue args;
    args.AppendString(kCallbackId);
    args.AppendString(kTestHost1);
    handler()->HandleGetCookieDetails(&args);
    task_environment()->RunUntilIdle();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    base::Value::ConstListView cookies_list = data.arg3()->GetList();
    ASSERT_EQ(2UL, cookies_list.size());
    // Find the entry item associated with the kTestCookie1 cookie.
    for (const auto& cookie : cookies_list) {
      if (cookie.FindKey("type")->GetString() == "cookie")
        node_path_id = cookie.FindKey("idPath")->GetString();
    }
  }

  // Remove path and ensure that the removed item listener fires.
  {
    base::ListValue args;
    args.AppendString(node_path_id);
    handler()->HandleRemoveItem(&args);
    task_environment()->RunUntilIdle();

    // Removal should fire an update event.
    const content::TestWebUI::CallData& all_data =
        *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", all_data.function_name());
    EXPECT_EQ("on-tree-item-removed", all_data.arg1()->GetString());
  }

  // Ensure that the removed item is no longer present in cookie details.
  {
    base::ListValue args;
    args.AppendString(kCallbackId);
    args.AppendString(kTestHost1);
    handler()->HandleGetCookieDetails(&args);
    task_environment()->RunUntilIdle();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    base::Value::ConstListView cookies_list = data.arg3()->GetList();
    ASSERT_EQ(1UL, cookies_list.size());
    EXPECT_EQ("local_storage", cookies_list[0].FindKey("type")->GetString());
  }
}

TEST_F(CookiesViewHandlerTest, HandleRemoveSite) {
  SetupHandlerWithTreeModel();

  // Check that removing a single site works.
  {
    base::ListValue args;
    args.AppendString(kTestHost1);
    handler()->HandleRemoveSite(&args);
    task_environment()->RunUntilIdle();

    // Removal should fire an update event.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    EXPECT_EQ("on-tree-item-removed", data.arg1()->GetString());
  }

  // Check that the removed site is no longer present in the display list.
  {
    base::ListValue args;
    args.AppendString(kCallbackId);
    args.AppendString("");
    handler()->HandleGetDisplayList(&args);
    task_environment()->RunUntilIdle();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    base::Value::ConstListView local_data_list = data.arg3()->GetList();
    ASSERT_EQ(1U, local_data_list.size());
    EXPECT_EQ(kTestHost2, local_data_list[0].FindKey("site")->GetString());
  }
}

}  // namespace settings
