// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/preview_server_proxy.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/data_sharing/public/features.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace data_sharing {
namespace {

const char kTestServerUrl[] = "https://test.com";
const char kCollaborationId[] = "resources/1234567/e/111111111111111";
const char kAccessToken[] = "abcdefg";
const char kExpectedUrl[] =
    "https://test.com/collaborations/"
    "cmVzb3VyY2VzLzEyMzQ1NjcvZS8xMTExMTExMTExMTExMTE=/dataTypes/-/"
    "sharedEntities:preview?accessToken=abcdefg&pageToken=&pageSize=100";

const std::string kTabGroupResponse = R"(
  {
    "sharedEntities":[
      {
        "clientTagHash":"abc",
        "collaboration":{
          "collaborationId":"resources/1234567/e/111111111111111"
        },
        "specifics":{
          "sharedTabGroupData":{
            "guid":"33333",
            "tabGroup":{
              "color":"GREY",
              "title":"Test"
            },
            "updateTimeWindowsEpochMicros":"100"
          }
        },
        "createTime":{"nanos":312000000,"seconds":"1721982740"},
        "deleted":false,
        "name":"sample",
        "updateTime":{"nanos":313560000,"seconds":"1721982801"},
        "version":"1234"
      }
    ]
  })";

const std::string kTabResponse = R"(
  {
    "sharedEntities":[
      {
        "clientTagHash":"abc",
        "collaboration":{
          "collaborationId":"resources/1234567/e/111111111111111"
        },
        "specifics":{
          "sharedTabGroupData":{
            "guid":"22222",
            "tab":{
              "sharedTabGroupGuid":"33333",
              "title":"foo",
              "url":"https://www.foo.com/"
            },
            "updateTimeWindowsEpochMicros":"200"
          }
        },
        "createTime":{"nanos":312000000,"seconds":"1721982740"},
        "deleted":false,
        "name":"sample",
        "updateTime":{"nanos":313560000,"seconds":"1721982801"},
        "version":"1234"
      }
    ]
  })";

const std::string kNoEntitySpecificsResponse = R"(
  {
    "sharedEntities":[
      {
        "clientTagHash":"abc",
        "collaboration":{
          "collaborationId":"resources/1234567/e/111111111111111"
        },
        "createTime":{"nanos":312000000,"seconds":"1721982740"},
        "deleted":false,
        "name":"sample",
        "updateTime":{"nanos":313560000,"seconds":"1721982801"},
        "version":"1234"
      }
    ]
  })";

class FakePreviewServerProxy : public PreviewServerProxy {
 public:
  FakePreviewServerProxy(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : PreviewServerProxy(identity_manager, url_loader_factory) {}
  FakePreviewServerProxy(const FakePreviewServerProxy&) = delete;
  FakePreviewServerProxy operator=(const FakePreviewServerProxy&) = delete;
  ~FakePreviewServerProxy() override = default;
  MOCK_METHOD(std::unique_ptr<EndpointFetcher>,
              CreateEndpointFetcher,
              (const GURL& url),
              (override));
};

class PreviewServerProxyTest : public testing::Test {
 protected:
  void SetUp() override {
    base::FieldTrialParams params;
    params["preview_service_base_url"] = kTestServerUrl;
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kDataSharingFeature, params);
    fetcher_ = std::make_unique<MockEndpointFetcher>();
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    server_proxy_ = std::make_unique<FakePreviewServerProxy>(
        identity_test_env_.identity_manager(),
        std::move(test_url_loader_factory));
    ON_CALL(*server_proxy_, CreateEndpointFetcher).WillByDefault([this]() {
      return std::move(fetcher_);
    });
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<MockEndpointFetcher> fetcher_;
  std::unique_ptr<FakePreviewServerProxy> server_proxy_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_TabGroup) {
  fetcher_->SetFetchResponse(kTabGroupResponse);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value().shared_entities.size(), 1u);
        SharedEntity entity = result.value().shared_entities[0];
        ASSERT_EQ(entity.group_id.value(),
                  "resources/1234567/e/111111111111111");
        ASSERT_EQ(entity.name, "sample");
        ASSERT_EQ(entity.version, 1234);
        ASSERT_EQ(entity.client_tag_hash, "abc");
        ASSERT_EQ(entity.create_time,
                  base::Time::FromSecondsSinceUnixEpoch(1721982740.312));
        ASSERT_EQ(entity.update_time,
                  base::Time::FromSecondsSinceUnixEpoch(1721982801.31356));
        ASSERT_TRUE(entity.specifics.has_shared_tab_group_data());
        sync_pb::SharedTabGroupDataSpecifics specifics;
        specifics.Swap(entity.specifics.mutable_shared_tab_group_data());
        ASSERT_EQ(specifics.guid(), "33333");
        ASSERT_EQ(specifics.update_time_windows_epoch_micros(), 100);
        ASSERT_TRUE(specifics.has_tab_group());
        ASSERT_EQ(specifics.tab_group().color(), sync_pb::SharedTabGroup::GREY);
        ASSERT_EQ(specifics.tab_group().title(), "Test");
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_Tab) {
  fetcher_->SetFetchResponse(kTabResponse);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      base::BindOnce(
          [](const DataSharingService::SharedDataPreviewOrFailureOutcome&
                 result) {
            ASSERT_TRUE(result.has_value());
            ASSERT_EQ(result.value().shared_entities.size(), 1u);
            SharedEntity entity = result.value().shared_entities[0];
            ASSERT_TRUE(entity.specifics.has_shared_tab_group_data());
            sync_pb::SharedTabGroupDataSpecifics specifics;
            specifics.Swap(entity.specifics.mutable_shared_tab_group_data());
            ASSERT_EQ(specifics.guid(), "22222");
            ASSERT_EQ(specifics.update_time_windows_epoch_micros(), 200);
            ASSERT_TRUE(specifics.has_tab());
            ASSERT_EQ(specifics.tab().shared_tab_group_guid(), "33333");
            ASSERT_EQ(specifics.tab().title(), "foo");
            ASSERT_EQ(specifics.tab().url(), "https://www.foo.com/");
          })
          .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest,
       TestGetSharedDataPreview_ErrorWithoutEntitySpecifics) {
  fetcher_->SetFetchResponse(kNoEntitySpecificsResponse);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(
            result.error(),
            DataSharingService::PeopleGroupActionFailure::kPersistentFailure);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_Deleted) {
  std::string response = kTabGroupResponse;
  base::ReplaceFirstSubstringAfterOffset(&response, 0, "\"deleted\":false",
                                         "\"deleted\":true");
  fetcher_->SetFetchResponse(response);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(
            result.error(),
            DataSharingService::PeopleGroupActionFailure::kPersistentFailure);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_ServerError) {
  fetcher_->SetFetchResponse("", net::HTTP_NOT_FOUND);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(
            result.error(),
            DataSharingService::PeopleGroupActionFailure::kTransientFailure);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_WrongJson) {
  fetcher_->SetFetchResponse("wrong");
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(
            result.error(),
            DataSharingService::PeopleGroupActionFailure::kPersistentFailure);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace
}  // namespace data_sharing
