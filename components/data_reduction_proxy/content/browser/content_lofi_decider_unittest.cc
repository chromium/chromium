// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/previews_state.h"
#include "net/base/load_flags.h"
#include "net/base/network_delegate_impl.h"
#include "net/http/http_request_headers.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

#if defined(OS_ANDROID)
const Client kClient = Client::CHROME_ANDROID;
#elif defined(OS_IOS)
const Client kClient = Client::CHROME_IOS;
#elif defined(OS_MACOSX)
const Client kClient = Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
const Client kClient = Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
const Client kClient = Client::CHROME_LINUX;
#elif defined(OS_WIN)
const Client kClient = Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
const Client kClient = Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
const Client kClient = Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
const Client kClient = Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
const Client kClient = Client::CHROME_QNX;
#else
const Client kClient = Client::UNKNOWN;
#endif

}  // namespace

class ContentLoFiDeciderTest : public testing::Test {
 public:
  ContentLoFiDeciderTest() : context_(false) {
    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithClient(kClient)
                        .WithURLRequestContext(&context_)
                        .Build();

    data_reduction_proxy_network_delegate_.reset(
        new DataReductionProxyNetworkDelegate(
            std::unique_ptr<net::NetworkDelegate>(
                new net::NetworkDelegateImpl()),
            test_context_->config(),
            test_context_->io_data()->request_options(),
            test_context_->configurator()));

    data_reduction_proxy_network_delegate_->InitIODataAndUMA(
        test_context_->io_data(), test_context_->io_data()->bypass_stats());


    std::unique_ptr<data_reduction_proxy::ContentLoFiDecider>
        data_reduction_proxy_lofi_decider(
            new data_reduction_proxy::ContentLoFiDecider());
    test_context_->io_data()->set_lofi_decider(
        std::move(data_reduction_proxy_lofi_decider));
  }

  void AllocateRequestInfoForTesting(net::URLRequest* request,
                                     content::ResourceType resource_type,
                                     content::PreviewsState previews_state) {
    content::ResourceRequestInfo::AllocateForTesting(
        request, resource_type, nullptr, -1, -1, -1,
        resource_type == content::RESOURCE_TYPE_MAIN_FRAME,
        false,  // allow_download
        false,  // is_async
        previews_state,
        nullptr);  // navigation_ui_data
  }

  std::unique_ptr<net::URLRequest> CreateRequest(
      bool is_main_frame,
      content::PreviewsState previews_state) {
    std::unique_ptr<net::URLRequest> request =
        context_.CreateRequest(GURL("http://www.google.com/"), net::IDLE,
                               &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
    AllocateRequestInfoForTesting(
        request.get(),
        (is_main_frame ? content::RESOURCE_TYPE_MAIN_FRAME
                       : content::RESOURCE_TYPE_SUB_FRAME),
        previews_state);
    return request;
  }

  std::unique_ptr<net::URLRequest> CreateRequestByType(
      content::ResourceType resource_type,
      bool scheme_is_https,
      content::PreviewsState previews_state) {
    std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
        GURL(scheme_is_https ? "https://www.google.com/"
                             : "http://www.google.com/"),
        net::IDLE, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
    AllocateRequestInfoForTesting(request.get(), resource_type, previews_state);
    return request;
  }

  void DelegateStageDone(int result) {}

  void NotifyBeforeSendHeaders(net::HttpRequestHeaders* headers,
                               net::URLRequest* request,
                               bool use_data_reduction_proxy) {
    net::ProxyInfo data_reduction_proxy_info;
    net::ProxyRetryInfoMap proxy_retry_info;

    if (use_data_reduction_proxy) {
      test_context_->config()->test_params()->UseNonSecureProxiesForHttp();
      data_reduction_proxy_info.UseProxyServer(test_context_->config()
                                                   ->test_params()
                                                   ->proxies_for_http()
                                                   .front()
                                                   .proxy_server());

    } else {
      data_reduction_proxy_info.UseNamedProxy("proxy.com");
    }

    data_reduction_proxy_network_delegate_->NotifyBeforeStartTransaction(
        request,
        base::BindOnce(&ContentLoFiDeciderTest::DelegateStageDone,
                       base::Unretained(this)),
        headers);
    data_reduction_proxy_network_delegate_->NotifyBeforeSendHeaders(
        request, data_reduction_proxy_info, proxy_retry_info, headers);
  }

  static void VerifyLoFiHeader(bool expected_lofi_used,
                               const net::HttpRequestHeaders& headers) {
    if (expected_lofi_used)
      EXPECT_TRUE(headers.HasHeader(chrome_proxy_accept_transform_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(expected_lofi_used, header_value == empty_image_directive());
  }

  static void VerifyLitePageHeader(bool expected_lofi_preview_used,
                                   const net::HttpRequestHeaders& headers) {
    if (expected_lofi_preview_used)
      EXPECT_TRUE(headers.HasHeader(chrome_proxy_accept_transform_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(expected_lofi_preview_used,
              header_value == lite_page_directive());
  }

  static void VerifyAcceptTransformHeader(const net::URLRequest& request,
                                          bool expected_accept_lite_page,
                                          bool expected_accept_empty_image) {
    std::unique_ptr<data_reduction_proxy::ContentLoFiDecider> lofi_decider(
        new data_reduction_proxy::ContentLoFiDecider());
    net::HttpRequestHeaders headers;
    lofi_decider->MaybeSetAcceptTransformHeader(request, &headers);

    std::string header_value;
    EXPECT_EQ(expected_accept_lite_page || expected_accept_empty_image,
              headers.GetHeader(chrome_proxy_accept_transform_header(),
                                &header_value));
    if (expected_accept_lite_page) {
      EXPECT_TRUE(header_value == lite_page_directive());
    } else if (expected_accept_empty_image) {
      EXPECT_TRUE(header_value == empty_image_directive());
    }
  }

  static void VerifyVideoHeader(bool expected_compressed_video_used,
                                const net::HttpRequestHeaders& headers) {
    EXPECT_EQ(expected_compressed_video_used,
              headers.HasHeader(chrome_proxy_accept_transform_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(
        expected_compressed_video_used,
        header_value.find(compressed_video_directive()) != std::string::npos);
  }

 protected:
  base::MessageLoopForIO message_loop_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<DataReductionProxyNetworkDelegate>
      data_reduction_proxy_network_delegate_;
};

TEST_F(ContentLoFiDeciderTest, MaybeSetAcceptTransformNoAcceptForPreviewsOff) {
  // Turn on proxy-decides-transform feature for these unit tests.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataReductionProxyDecidesTransform);

  std::unique_ptr<net::URLRequest> request =
      CreateRequest(true /* is main */, content::PREVIEWS_OFF);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              false /* empty-image */);

  request = CreateRequest(true /* is main */, content::PREVIEWS_NO_TRANSFORM);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              false /* empty-image */);

  request = CreateRequest(true /* is main */, content::PREVIEWS_UNSPECIFIED);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              false /* empty-image */);
}

TEST_F(ContentLoFiDeciderTest, MaybeSetAcceptTransformNoAcceptForHttps) {
  // Turn on proxy-decides-transform feature for these unit tests.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataReductionProxyDecidesTransform);

  content::PreviewsState both_previews_enabled =
      content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON;

  // Verify no accept header for HTTPS.
  std::unique_ptr<net::URLRequest> request =
      CreateRequestByType(content::RESOURCE_TYPE_MAIN_FRAME, true /* https */,
                          both_previews_enabled);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              false /* empty-image */);

  request = CreateRequestByType(content::RESOURCE_TYPE_IMAGE, true /* https */,
                                both_previews_enabled);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              false /* empty-image */);
}

TEST_F(ContentLoFiDeciderTest, MaybeSetAcceptTransformHeaderAcceptLitePage) {
  // Turn on proxy-decides-transform feature for these unit tests.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataReductionProxyDecidesTransform);

  content::PreviewsState lite_page_enabled = content::SERVER_LITE_PAGE_ON;

  // Verify accepting lite-page per resource type.
  std::unique_ptr<net::URLRequest> request =
      CreateRequest(true /* is main */, lite_page_enabled);
  VerifyAcceptTransformHeader(*request, true /* lite-page */,
                              false /* empty-image */);

  request = CreateRequest(false /* is main */, lite_page_enabled);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              false /* empty-image */);
}

TEST_F(ContentLoFiDeciderTest, MaybeSetAcceptTransformHeaderAcceptEmptyImage) {
  // Turn on proxy-decides-transform feature for these unit tests.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataReductionProxyDecidesTransform);

  content::PreviewsState lofi_enabled = content::SERVER_LOFI_ON;

  // Verify accepting empty-image per resource type.
  std::unique_ptr<net::URLRequest> request = CreateRequestByType(
      content::RESOURCE_TYPE_MAIN_FRAME, false /* https */, lofi_enabled);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              false /* empty-image */);

  request = CreateRequestByType(content::RESOURCE_TYPE_IMAGE, false /* https */,
                                lofi_enabled);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              true /* empty-image */);

  request = CreateRequestByType(content::RESOURCE_TYPE_FAVICON,
                                false /* https */, lofi_enabled);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              true /* empty-image */);

  request = CreateRequestByType(content::RESOURCE_TYPE_SCRIPT,
                                false /* https */, lofi_enabled);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              false /* empty-image */);

  request = CreateRequestByType(content::RESOURCE_TYPE_STYLESHEET,
                                false /* https */, lofi_enabled);
  VerifyAcceptTransformHeader(*request, false /* lite-page */,
                              false /* empty-image */);
}

TEST_F(ContentLoFiDeciderTest, AcceptTransformPerResourceType) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataReductionProxyDecidesTransform);

  const struct {
    content::ResourceType resource_type;
  } tests[] = {{content::RESOURCE_TYPE_MAIN_FRAME},
               {content::RESOURCE_TYPE_SUB_FRAME},
               {content::RESOURCE_TYPE_STYLESHEET},
               {content::RESOURCE_TYPE_SCRIPT},
               {content::RESOURCE_TYPE_IMAGE},
               {content::RESOURCE_TYPE_FONT_RESOURCE},
               {content::RESOURCE_TYPE_SUB_RESOURCE},
               {content::RESOURCE_TYPE_OBJECT},
               {content::RESOURCE_TYPE_MEDIA},
               {content::RESOURCE_TYPE_WORKER},
               {content::RESOURCE_TYPE_SHARED_WORKER},
               {content::RESOURCE_TYPE_PREFETCH},
               {content::RESOURCE_TYPE_FAVICON},
               {content::RESOURCE_TYPE_XHR},
               {content::RESOURCE_TYPE_PING},
               {content::RESOURCE_TYPE_SERVICE_WORKER},
               {content::RESOURCE_TYPE_CSP_REPORT},
               {content::RESOURCE_TYPE_PLUGIN_RESOURCE}};

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request = CreateRequestByType(
        tests[i].resource_type, false,
        content::SERVER_LOFI_ON | content::SERVER_LITE_PAGE_ON);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);

    bool is_main_frame =
        tests[i].resource_type == content::RESOURCE_TYPE_MAIN_FRAME;
    bool is_lofi_resource_type =
        !(tests[i].resource_type == content::RESOURCE_TYPE_MAIN_FRAME ||
          tests[i].resource_type == content::RESOURCE_TYPE_STYLESHEET ||
          tests[i].resource_type == content::RESOURCE_TYPE_SCRIPT ||
          tests[i].resource_type == content::RESOURCE_TYPE_FONT_RESOURCE ||
          tests[i].resource_type == content::RESOURCE_TYPE_MEDIA ||
          tests[i].resource_type == content::RESOURCE_TYPE_CSP_REPORT);

    VerifyLitePageHeader(is_main_frame, headers);
    VerifyLoFiHeader(is_lofi_resource_type, headers);
  }
}

TEST_F(ContentLoFiDeciderTest, ProxyIsNotDataReductionProxy) {
  const struct {
    content::PreviewsState previews_state;
  } tests[] = {
      {content::PREVIEWS_OFF}, {content::SERVER_LOFI_ON},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request =
        CreateRequest(false, tests[i].previews_state);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), false);
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(std::string::npos, header_value.find(empty_image_directive()));
  }
}

TEST_F(ContentLoFiDeciderTest, VideoDirectiveNotOverridden) {
  // Verify the directive gets added even when LoFi is triggered.
  std::unique_ptr<net::URLRequest> request =
      CreateRequestByType(content::RESOURCE_TYPE_MEDIA, false, true);
  net::HttpRequestHeaders headers;
  NotifyBeforeSendHeaders(&headers, request.get(), true);
  VerifyVideoHeader(true, headers);
}

TEST_F(ContentLoFiDeciderTest, VideoDirectiveNotAdded) {
  std::unique_ptr<net::URLRequest> request =
      CreateRequestByType(content::RESOURCE_TYPE_MEDIA, false, true);
  net::HttpRequestHeaders headers;
  // Verify the header isn't there when the data reduction proxy is disabled.
  NotifyBeforeSendHeaders(&headers, request.get(), false);
  VerifyVideoHeader(false, headers);
}

TEST_F(ContentLoFiDeciderTest, VideoDirectiveDoesNotOverride) {
  // Verify the directive gets added even when LoFi is triggered.
  std::unique_ptr<net::URLRequest> request =
      CreateRequestByType(content::RESOURCE_TYPE_MEDIA, false, true);
  net::HttpRequestHeaders headers;
  headers.SetHeader(chrome_proxy_accept_transform_header(), "empty-image");
  NotifyBeforeSendHeaders(&headers, request.get(), true);
  std::string header_value;
  headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
  EXPECT_EQ("empty-image", header_value);
}

TEST_F(ContentLoFiDeciderTest, RemoveAcceptTransformHeader) {
  std::unique_ptr<data_reduction_proxy::ContentLoFiDecider> lofi_decider(
      new data_reduction_proxy::ContentLoFiDecider());
  net::HttpRequestHeaders headers;
  headers.SetHeader(chrome_proxy_accept_transform_header(), "Foo");
  EXPECT_TRUE(headers.HasHeader(chrome_proxy_accept_transform_header()));
  lofi_decider->RemoveAcceptTransformHeader(&headers);
  EXPECT_FALSE(headers.HasHeader(chrome_proxy_accept_transform_header()));
}

TEST_F(ContentLoFiDeciderTest, NoTransformDoesNotAddHeader) {
  std::unique_ptr<net::URLRequest> request =
      CreateRequest(false, content::PREVIEWS_NO_TRANSFORM);
  net::HttpRequestHeaders headers;
  NotifyBeforeSendHeaders(&headers, request.get(), true);
  EXPECT_FALSE(headers.HasHeader(chrome_proxy_accept_transform_header()));
}

TEST_F(ContentLoFiDeciderTest, RequestIsClientSideLoFiMainFrameTest) {
  std::unique_ptr<net::URLRequest> request = CreateRequestByType(
      content::RESOURCE_TYPE_MAIN_FRAME, true, content::CLIENT_LOFI_ON);
  std::unique_ptr<data_reduction_proxy::ContentLoFiDecider> lofi_decider(
      new data_reduction_proxy::ContentLoFiDecider());
  EXPECT_FALSE(lofi_decider->IsClientLoFiImageRequest(*request));
}

TEST_F(ContentLoFiDeciderTest, RequestIsNotClientSideLoFiImageTest) {
  std::unique_ptr<net::URLRequest> request = CreateRequestByType(
      content::RESOURCE_TYPE_IMAGE, true, content::PREVIEWS_NO_TRANSFORM);
  std::unique_ptr<data_reduction_proxy::ContentLoFiDecider> lofi_decider(
      new data_reduction_proxy::ContentLoFiDecider());
  EXPECT_FALSE(lofi_decider->IsClientLoFiImageRequest(*request));
}

TEST_F(ContentLoFiDeciderTest, RequestIsClientSideLoFiImageTest) {
  std::unique_ptr<net::URLRequest> request = CreateRequestByType(
      content::RESOURCE_TYPE_IMAGE, true, content::CLIENT_LOFI_ON);
  std::unique_ptr<data_reduction_proxy::ContentLoFiDecider> lofi_decider(
      new data_reduction_proxy::ContentLoFiDecider());
  EXPECT_TRUE(lofi_decider->IsClientLoFiImageRequest(*request));
}

TEST_F(ContentLoFiDeciderTest, RequestIsClientLoFiAutoReload) {
  // IsClientLoFiAutoReloadRequest() should return true for any request with the
  // CLIENT_LOFI_AUTO_RELOAD bit set.

  EXPECT_TRUE(ContentLoFiDecider().IsClientLoFiAutoReloadRequest(
      *CreateRequestByType(content::RESOURCE_TYPE_IMAGE, false,
                           content::CLIENT_LOFI_AUTO_RELOAD)));

  EXPECT_TRUE(
      ContentLoFiDecider().IsClientLoFiAutoReloadRequest(*CreateRequestByType(
          content::RESOURCE_TYPE_IMAGE, true,
          content::CLIENT_LOFI_AUTO_RELOAD | content::PREVIEWS_NO_TRANSFORM)));

  EXPECT_TRUE(ContentLoFiDecider().IsClientLoFiAutoReloadRequest(
      *CreateRequestByType(content::RESOURCE_TYPE_MAIN_FRAME, true,
                           content::CLIENT_LOFI_AUTO_RELOAD)));

  EXPECT_TRUE(ContentLoFiDecider().IsClientLoFiAutoReloadRequest(
      *CreateRequestByType(content::RESOURCE_TYPE_SCRIPT, true,
                           content::CLIENT_LOFI_AUTO_RELOAD)));

  // IsClientLoFiAutoReloadRequest() should return false for any request without
  // the CLIENT_LOFI_AUTO_RELOAD bit set.
  EXPECT_FALSE(ContentLoFiDecider().IsClientLoFiAutoReloadRequest(
      *CreateRequestByType(content::RESOURCE_TYPE_IMAGE, false,
                           content::PREVIEWS_NO_TRANSFORM)));
}

TEST_F(ContentLoFiDeciderTest, DetermineCommittedServerPreviewsStateLitePage) {
  content::PreviewsState enabled_previews =
      content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
      content::CLIENT_LOFI_ON | content::NOSCRIPT_ON;

  // Add DataReductionProxyData for LitePage to URLRequest.
  data_reduction_proxy::DataReductionProxyData data_reduction_proxy_data;
  data_reduction_proxy_data.set_used_data_reduction_proxy(true);
  data_reduction_proxy_data.set_lite_page_received(true);
  data_reduction_proxy_data.set_lofi_policy_received(false);

  // Verify selects LitePage bit but doesn't touch client-only NoScript bit.
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON | content::NOSCRIPT_ON,
            ContentLoFiDecider::DetermineCommittedServerPreviewsState(
                &data_reduction_proxy_data, enabled_previews));
}

TEST_F(ContentLoFiDeciderTest, DetermineCommittedServerPreviewsStateLoFi) {
  content::PreviewsState enabled_previews =
      content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
      content::CLIENT_LOFI_ON | content::NOSCRIPT_ON;

  // Add DataReductionProxyData for LitePage to URLRequest.
  data_reduction_proxy::DataReductionProxyData data_reduction_proxy_data;
  data_reduction_proxy_data.set_used_data_reduction_proxy(true);
  data_reduction_proxy_data.set_lite_page_received(false);
  data_reduction_proxy_data.set_lofi_policy_received(true);

  // Verify keeps LoFi bits and also doesn't touch client-only NoScript bit.
  EXPECT_EQ(
      content::SERVER_LOFI_ON | content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
      ContentLoFiDecider::DetermineCommittedServerPreviewsState(
          &data_reduction_proxy_data, enabled_previews));
}

TEST_F(ContentLoFiDeciderTest, DetermineCommittedServerPreviewsStateNoProxy) {
  content::PreviewsState enabled_previews =
      content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
      content::CLIENT_LOFI_ON | content::NOSCRIPT_ON;

  // Verify keeps LoFi bits and also doesn't touch client-only NoScript bit.
  EXPECT_EQ(content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
            ContentLoFiDecider::DetermineCommittedServerPreviewsState(
                nullptr, enabled_previews));
}

}  // namespace data_reduction_proxy
