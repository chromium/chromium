// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/preview_server_proxy.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "components/data_sharing/public/features.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using endpoint_fetcher::MockEndpointFetcher;
using testing::_;

namespace data_sharing {
namespace {

const char kCollaborationId[] = "resources/1234567/e/111111111111111";
const char kAccessToken[] = "abcdefg";
const char kExpectedUrl[] =
    "https://staging-chromesyncsharedentities-pa.googleapis.com/v1/"
    "collaborations/"
    "cmVzb3VyY2VzLzEyMzQ1NjcvZS8xMTExMTExMTExMTExMTE/dataTypes/"
    "-/"
    "sharedEntities:preview?accessToken=abcdefg&pageToken=&pageSize=550";
const char kExpectedUrlSharedTabGroupsOnly[] =
    "https://staging-chromesyncsharedentities-pa.googleapis.com/v1/"
    "collaborations/"
    "cmVzb3VyY2VzLzEyMzQ1NjcvZS8xMTExMTExMTExMTExMTE/dataTypes/"
    "1239418/"
    "sharedEntities:preview?accessToken=abcdefg&pageToken=&pageSize=550";
const char kExpectedUrlStableAndBeta[] =
    "https://chromesyncsharedentities-pa.googleapis.com/v1/"
    "collaborations/"
    "cmVzb3VyY2VzLzEyMzQ1NjcvZS8xMTExMTExMTExMTExMTE/dataTypes/-/"
    "sharedEntities:preview?accessToken=abcdefg&pageToken=&pageSize=550";
const char kExpectedUrlAutopush[] =
    "https://autopush-chromesyncsharedentities-pa.sandbox.googleapis.com/v1/"
    "collaborations/"
    "cmVzb3VyY2VzLzEyMzQ1NjcvZS8xMTExMTExMTExMTExMTE/dataTypes/-/"
    "sharedEntities:preview?accessToken=abcdefg&pageToken=&pageSize=550";
const char kExpectedUrlFieldTrial[] =
    "https://test.com/"
    "collaborations/"
    "cmVzb3VyY2VzLzEyMzQ1NjcvZS8xMTExMTExMTExMTExMTE/dataTypes/-/"
    "sharedEntities:preview?accessToken=abcdefg&pageToken=&pageSize=550";

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
      },
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
              "url":"https://www.foo.com/",
              "uniquePosition":{
                "customCompressedV1":"////3y9yTGFOaG80WEZacWlyNFZQUWkvWGlZME84cz0="
              }
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

const std::string kDasherPolicyViolationError = R"(
{
  "error":
    {
      "code": 403,
      "message": "The caller does not have permission",
      "status": "PERMISSION_DENIED",
      "details": [{
        "@type": "type.googleapis.com/google.rpc.ErrorInfo",
        "reason": "SAME_CUSTOMER_DASHER_POLICY_VIOLATED",
        "domain": "chromesyncsharedentities-pa.googleapis.com"
      }]
    }
  })";

class FakePreviewServerProxy : public PreviewServerProxy {
 public:
  FakePreviewServerProxy(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : PreviewServerProxy(identity_manager,
                           url_loader_factory,
                           version_info::Channel::DEFAULT) {}
  FakePreviewServerProxy(const FakePreviewServerProxy&) = delete;
  FakePreviewServerProxy operator=(const FakePreviewServerProxy&) = delete;
  ~FakePreviewServerProxy() override = default;

  // MOCK_METHOD GetChannel below is a templated function and not able to access
  // theJ PreviewServerProxy::GetChannel() method, so provide this method as a
  // redirect.
  version_info::Channel MockGetChannel() {
    return PreviewServerProxy::GetChannel();
  }

  MOCK_METHOD(std::unique_ptr<endpoint_fetcher::EndpointFetcher>,
              CreateEndpointFetcher,
              (const GURL& url),
              (override));
  MOCK_METHOD(version_info::Channel, GetChannel, (), (const override));
};

struct ChannelVariationTestData {
  version_info::Channel channel;
  std::string expected_url;
};

// Maps expected servers for each cannel.
const ChannelVariationTestData kChannelVariationTestData[] = {
    {version_info::Channel::STABLE, kExpectedUrlStableAndBeta},
    {version_info::Channel::BETA, kExpectedUrlStableAndBeta},
    {version_info::Channel::DEV, kExpectedUrl},
    {version_info::Channel::CANARY, kExpectedUrl},
    {version_info::Channel::DEFAULT, kExpectedUrl},
    {version_info::Channel::UNKNOWN, kExpectedUrl},
};

class PreviewServerProxyTest
    : public testing::TestWithParam<ChannelVariationTestData> {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kDataSharingFeature, GetFieldTrialParams());
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
    ON_CALL(*server_proxy_, GetChannel).WillByDefault([this]() {
      return server_proxy_->MockGetChannel();
    });
  }

  void QueryAndWaitForResponse(
      std::optional<syncer::DataType> data_type = std::nullopt) {
    base::RunLoop run_loop;
    server_proxy_->GetSharedDataPreview(
        GroupToken(GroupId(kCollaborationId), kAccessToken), data_type,
        base::BindOnce(
            [](const DataSharingService::SharedDataPreviewOrFailureOutcome&
                   result) { ASSERT_TRUE(result.has_value()); })
            .Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Provide this as a virtual methods to enable other tests to override
  // field trial params.
  virtual base::FieldTrialParams GetFieldTrialParams() {
    return base::FieldTrialParams();
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
      /*data_type=*/std::nullopt,
      base::BindOnce(
          [](const DataSharingService::SharedDataPreviewOrFailureOutcome&
                 result) {
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(result.value().shared_tab_group_preview);
            SharedTabGroupPreview preview =
                result.value().shared_tab_group_preview.value();
            ASSERT_EQ(preview.title, "Test");
            ASSERT_EQ(preview.tabs.size(), 1u);
            ASSERT_EQ(preview.tabs[0].url, GURL("https://www.foo.com/"));
          })
          .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_TabGroupsOnly) {
  fetcher_->SetFetchResponse(kTabGroupResponse);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kExpectedUrlSharedTabGroupsOnly)))
      .Times(1);

  QueryAndWaitForResponse(syncer::DataType::SHARED_TAB_GROUP_DATA);
}

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_TabWithoutGroup) {
  fetcher_->SetFetchResponse(kTabResponse);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      /*data_type=*/std::nullopt,
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(result.error(),
                  DataSharingService::DataPreviewActionFailure::kOtherFailure);
      }).Then(run_loop.QuitClosure()));
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
      /*data_type=*/std::nullopt,
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(result.error(),
                  DataSharingService::DataPreviewActionFailure::kOtherFailure);
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
      /*data_type=*/std::nullopt,
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(result.error(),
                  DataSharingService::DataPreviewActionFailure::kOtherFailure);
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
      /*data_type=*/std::nullopt,
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(result.error(),
                  DataSharingService::DataPreviewActionFailure::kOtherFailure);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_PermissionError) {
  fetcher_->SetFetchResponse("", net::HTTP_FORBIDDEN);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      /*data_type=*/std::nullopt,
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(
            result.error(),
            DataSharingService::DataPreviewActionFailure::kPermissionDenied);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest,
       TestGetSharedDataPreview_PermissionErrorDasherViolation) {
  fetcher_->SetFetchResponse(kDasherPolicyViolationError, net::HTTP_FORBIDDEN);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      /*data_type=*/std::nullopt,
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(result.error(), DataSharingService::DataPreviewActionFailure::
                                      kGroupClosedByOrganizationPolicy);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_ResourceExceeded) {
  fetcher_->SetFetchResponse("", net::HTTP_CONFLICT);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      /*data_type=*/std::nullopt,
      base::BindOnce(
          [](const DataSharingService::SharedDataPreviewOrFailureOutcome&
                 result) {
            ASSERT_EQ(result.error(),
                      DataSharingService::DataPreviewActionFailure::kGroupFull);
          })
          .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PreviewServerProxyTest, TestGetSharedDataPreview_WrongJson) {
  fetcher_->SetFetchResponse("wrong");
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrl)))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->GetSharedDataPreview(
      GroupToken(GroupId(kCollaborationId), kAccessToken),
      /*data_type=*/std::nullopt,
      base::BindOnce([](const DataSharingService::
                            SharedDataPreviewOrFailureOutcome& result) {
        ASSERT_EQ(result.error(),
                  DataSharingService::DataPreviewActionFailure::kOtherFailure);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_P(PreviewServerProxyTest, TestGetSharedDataPreview) {
  fetcher_->SetFetchResponse(kTabGroupResponse);

  EXPECT_CALL(*server_proxy_, GetChannel)
      .WillRepeatedly(testing::Return(GetParam().channel));
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(GetParam().expected_url)))
      .Times(1);
  QueryAndWaitForResponse();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PreviewServerProxyTest,
                         testing::ValuesIn(kChannelVariationTestData));

TEST_F(PreviewServerProxyTest,
       TestGetSharedDataPreview_ChannelVariations_Default_WithCommandLine) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      syncer::kSyncServiceURL, "https//arbitrary-server.google.com/");
  fetcher_->SetFetchResponse(kTabGroupResponse);

  // For all builds, we want to use the autopush server when the sync sever is
  // manually specified.
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrlAutopush)))
      .Times(1);
  QueryAndWaitForResponse();
}

TEST_F(PreviewServerProxyTest,
       TestGetSharedDataPreview_ChannelVariations_Stable_WithCommandLine) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      syncer::kSyncServiceURL, "https//arbitrary-server.google.com/");
  fetcher_->SetFetchResponse(kTabGroupResponse);
  EXPECT_CALL(*server_proxy_, GetChannel)
      .WillRepeatedly(testing::Return(version_info::Channel::STABLE));

  // Even for stable channel builds, we want to use the autopush server when
  // the sync sever is manually specified.
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher(GURL(kExpectedUrlAutopush)))
      .Times(1);
  QueryAndWaitForResponse();
}

// When a field trial sets the preview service URL, we should always use that,
// regardless of channel or command line.
class FieldTrialPreviewServerProxyTest : public PreviewServerProxyTest {
 public:
  base::FieldTrialParams GetFieldTrialParams() override {
    base::FieldTrialParams params;
    params["preview_service_base_url"] = kExpectedUrlFieldTrial;
    return params;
  }
};

TEST_F(FieldTrialPreviewServerProxyTest,
       TestGetSharedDataPreview_StableChannel) {
  fetcher_->SetFetchResponse(kTabGroupResponse);

  EXPECT_CALL(*server_proxy_, GetChannel)
      .WillRepeatedly(testing::Return(version_info::Channel::STABLE));

  // Should not use stable server, but instead use from field trial.
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kExpectedUrlFieldTrial)))
      .Times(1);
  QueryAndWaitForResponse();
}

TEST_F(FieldTrialPreviewServerProxyTest, TestGetSharedDataPreview_CommandLine) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      syncer::kSyncServiceURL, "https//arbitrary-server.google.com/");
  fetcher_->SetFetchResponse(kTabGroupResponse);

  // Should not use server from command line but instead use from field trial.
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kExpectedUrlFieldTrial)))
      .Times(1);
  QueryAndWaitForResponse();
}

}  // namespace
}  // namespace data_sharing
