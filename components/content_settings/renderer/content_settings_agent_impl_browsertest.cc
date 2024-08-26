// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/renderer/content_settings_agent_impl.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/test/test_web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_view.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace content_settings {
namespace {

constexpr char kAllowlistScheme[] = "foo";

class MockContentSettingsManagerImpl : public mojom::ContentSettingsManager {
 public:
  struct Log {
    int allow_storage_access_count = 0;
    int on_content_blocked_count = 0;
    ContentSettingsType on_content_blocked_type = ContentSettingsType::DEFAULT;
  };

  explicit MockContentSettingsManagerImpl(Log* log) : log_(log) {}
  ~MockContentSettingsManagerImpl() override = default;

  // mojom::ContentSettingsManager methods:
  void Clone(
      mojo::PendingReceiver<mojom::ContentSettingsManager> receiver) override {
    ADD_FAILURE() << "Not reached";
  }
  void AllowStorageAccess(const blink::LocalFrameToken& frame_token,
                          StorageType storage_type,
                          const url::Origin& origin,
                          const net::SiteForCookies& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          base::OnceCallback<void(bool)> callback) override {
    ++log_->allow_storage_access_count;
    std::move(callback).Run(true);
  }
  void OnContentBlocked(const blink::LocalFrameToken& frame_token,
                        ContentSettingsType type) override {
    ++log_->on_content_blocked_count;
    log_->on_content_blocked_type = type;
  }

 private:
  raw_ptr<Log> log_;
};

class MockContentSettingsAgentDelegate
    : public ContentSettingsAgentImpl::Delegate {
 public:
  bool IsSchemeAllowlisted(const std::string& scheme) override {
    return scheme == kAllowlistScheme;
  }
};

class MockContentSettingsAgentImpl : public ContentSettingsAgentImpl {
 public:
  explicit MockContentSettingsAgentImpl(content::RenderFrame* render_frame);

  MockContentSettingsAgentImpl(const MockContentSettingsAgentImpl&) = delete;
  MockContentSettingsAgentImpl& operator=(const MockContentSettingsAgentImpl&) =
      delete;

  ~MockContentSettingsAgentImpl() override = default;

  const GURL& image_url() const { return image_url_; }
  const std::string& image_origin() const { return image_origin_; }

  // ContentSettingAgentImpl methods:
  void BindContentSettingsManager(
      mojo::Remote<mojom::ContentSettingsManager>* manager) override;

  int allow_storage_access_count() const {
    return log_.allow_storage_access_count;
  }
  int on_content_blocked_count() const { return log_.on_content_blocked_count; }
  ContentSettingsType on_content_blocked_type() const {
    return log_.on_content_blocked_type;
  }

 private:
  MockContentSettingsManagerImpl::Log log_;
  const GURL image_url_;
  const std::string image_origin_;
};

MockContentSettingsAgentImpl::MockContentSettingsAgentImpl(
    content::RenderFrame* render_frame)
    : ContentSettingsAgentImpl(
          render_frame,
          std::make_unique<MockContentSettingsAgentDelegate>()),
      image_url_("http://www.foo.com/image.jpg"),
      image_origin_("http://www.foo.com") {}

void MockContentSettingsAgentImpl::BindContentSettingsManager(
    mojo::Remote<mojom::ContentSettingsManager>* manager) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MockContentSettingsManagerImpl>(&log_),
      manager->BindNewPipeAndPassReceiver());
}

// Evaluates a boolean `predicate` every time a provisional load is committed in
// the given `frame` while the instance of this class is in scope, and verifies
// that the result matches the `expectation`.
class CommitTimeConditionChecker : public content::RenderFrameObserver {
 public:
  using Predicate = base::RepeatingCallback<bool()>;

  CommitTimeConditionChecker(content::RenderFrame* frame,
                             const Predicate& predicate,
                             bool expectation)
      : content::RenderFrameObserver(frame),
        predicate_(predicate),
        expectation_(expectation) {}

  CommitTimeConditionChecker(const CommitTimeConditionChecker&) = delete;
  CommitTimeConditionChecker& operator=(const CommitTimeConditionChecker&) =
      delete;

 protected:
  // RenderFrameObserver:
  void OnDestruct() override {}
  void DidCommitProvisionalLoad(ui::PageTransition transition) override {
    EXPECT_EQ(expectation_, predicate_.Run());
  }

 private:
  const Predicate predicate_;
  const bool expectation_;
};

}  // namespace

enum class BackgroundResourceFetchTestCase {
  kBackgroundResourceFetchEnabled,
  kBackgroundResourceFetchDisabled,
};

class ContentSettingsAgentImplBrowserTest
    : public content::RenderViewTest,
      public testing::WithParamInterface<BackgroundResourceFetchTestCase> {
 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsBackgroundResourceFetchEnabled()) {
      enabled_features.push_back(blink::features::kBackgroundResourceFetch);
    } else {
      disabled_features.push_back(blink::features::kBackgroundResourceFetch);
    }
    feature_background_resource_fetch_.InitWithFeatures(enabled_features,
                                                        disabled_features);
    RenderViewTest::SetUp();

    // Set up a fake url loader factory to ensure that script loader can create
    // a URLLoader.
    CreateFakeURLLoaderFactory();

    // Unbind the ContentSettingsAgent interface that would be registered by
    // the ContentSettingsAgentImpl created when the render frame is created.
    GetMainRenderFrame()->GetAssociatedInterfaceRegistry()->RemoveInterface(
        mojom::ContentSettingsAgent::Name_);

    // Bind a FakeCodeCacheHost which handles FetchCachedCode() method, because
    // script loading is blocked until the callback of FetchCachedCode() is
    // called.
    GetMainRenderFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        blink::mojom::CodeCacheHost::Name_,
        base::BindRepeating(
            &ContentSettingsAgentImplBrowserTest::OnCodeCacheHostRequest,
            base::Unretained(this)));
  }

 private:
  class FakeCodeCacheHost : public blink::mojom::CodeCacheHost {
   public:
    explicit FakeCodeCacheHost(
        mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver) {
      receiver_.Bind(std::move(receiver));
    }

    void DidGenerateCacheableMetadata(blink::mojom::CodeCacheType cache_type,
                                      const GURL& url,
                                      base::Time expected_response_time,
                                      mojo_base::BigBuffer data) override {}
    void FetchCachedCode(blink::mojom::CodeCacheType cache_type,
                         const GURL& url,
                         FetchCachedCodeCallback callback) override {
      std::move(callback).Run(base::Time(), std::vector<uint8_t>());
    }
    void ClearCodeCacheEntry(blink::mojom::CodeCacheType cache_type,
                             const GURL& url) override {}
    void DidGenerateCacheableMetadataInCacheStorage(
        const GURL& url,
        base::Time expected_response_time,
        mojo_base::BigBuffer data,
        const std::string& cache_storage_cache_name) override {}

   private:
    mojo::Receiver<blink::mojom::CodeCacheHost> receiver_{this};
  };

  bool IsBackgroundResourceFetchEnabled() const {
    return GetParam() ==
           BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled;
  }

  void OnCodeCacheHostRequest(mojo::ScopedMessagePipeHandle handle) {
    fake_code_cache_hosts_.emplace_back(std::make_unique<FakeCodeCacheHost>(
        mojo::PendingReceiver<blink::mojom::CodeCacheHost>(std::move(handle))));
  }

  std::vector<std::unique_ptr<FakeCodeCacheHost>> fake_code_cache_hosts_;
  base::test::ScopedFeatureList feature_background_resource_fetch_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContentSettingsAgentImplBrowserTest,
    testing::ValuesIn(
        {BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled,
         BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled}),
    [](const testing::TestParamInfo<BackgroundResourceFetchTestCase>& info) {
      switch (info.param) {
        case (BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled):
          return "BackgroundResourceFetchEnabled";
        case (
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled):
          return "BackgroundResourceFetchDisabled";
      }
    });

TEST_P(ContentSettingsAgentImplBrowserTest, DidBlockContentType) {
  MockContentSettingsAgentImpl mock_agent(GetMainRenderFrame());
  mock_agent.DidBlockContentType(ContentSettingsType::COOKIES);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.on_content_blocked_count());
  EXPECT_EQ(ContentSettingsType::COOKIES, mock_agent.on_content_blocked_type());

  // Blocking the same content type a second time shouldn't send a notification.
  mock_agent.DidBlockContentType(ContentSettingsType::COOKIES);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.on_content_blocked_count());
}

// Tests that multiple invocations of AllowStorageAccessSync result in a single
// IPC.
TEST_P(ContentSettingsAgentImplBrowserTest, AllowStorageAccessSync) {
  // Load some HTML, so we have a valid security origin.
  LoadHTMLWithUrlOverride("<html></html>", "https://example.com/");
  MockContentSettingsAgentImpl mock_agent(GetMainRenderFrame());
  mock_agent.AllowStorageAccessSync(
      blink::WebContentSettingsClient::StorageType::kLocalStorage);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.allow_storage_access_count());

  // Accessing localStorage from the same origin again shouldn't result in a
  // new IPC.
  mock_agent.AllowStorageAccessSync(
      blink::WebContentSettingsClient::StorageType::kLocalStorage);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.allow_storage_access_count());
}

// Tests that multiple invocations of AllowStorageAccess result in a single IPC.
TEST_P(ContentSettingsAgentImplBrowserTest, AllowStorageAccess) {
  // Load some HTML, so we have a valid security origin.
  LoadHTMLWithUrlOverride("<html></html>", "https://example.com/");
  MockContentSettingsAgentImpl mock_agent(GetMainRenderFrame());
  mock_agent.AllowStorageAccess(
      blink::WebContentSettingsClient::StorageType::kLocalStorage,
      base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.allow_storage_access_count());

  // Accessing localStorage from the same origin again shouldn't result in a
  // new IPC.
  mock_agent.AllowStorageAccess(
      blink::WebContentSettingsClient::StorageType::kLocalStorage,
      base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.allow_storage_access_count());
}

TEST_P(ContentSettingsAgentImplBrowserTest, MixedAutoupgradesDisabledByRules) {
  MockContentSettingsAgentImpl mock_agent(GetMainRenderFrame());

  LoadHTMLWithUrlOverride("<html></html>", "https://example.com/");

  // Set the default mixed content blocking setting.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& mixed_content_setting_rules =
      content_setting_rules.mixed_content_rules;
  mixed_content_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingToValue(CONTENT_SETTING_BLOCK), ProviderType::kNone,
      false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(GetMainRenderFrame());
  agent->SetRendererContentSettingRulesForTest(content_setting_rules);
  EXPECT_TRUE(agent->ShouldAutoupgradeMixedContent());

  // Create an exception which allows mixed content.
  mixed_content_setting_rules.insert(
      mixed_content_setting_rules.begin(),
      ContentSettingPatternSource(
          ContentSettingsPattern::FromString("https://example.com/"),
          ContentSettingsPattern::Wildcard(),
          ContentSettingToValue(CONTENT_SETTING_ALLOW), ProviderType::kNone,
          false));
  agent->SetRendererContentSettingRulesForTest(content_setting_rules);

  EXPECT_FALSE(agent->ShouldAutoupgradeMixedContent());
}

TEST_P(ContentSettingsAgentImplBrowserTest, MixedAutoupgradesNoSettingsSet) {
  MockContentSettingsAgentImpl mock_agent(GetMainRenderFrame());

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(GetMainRenderFrame());
  EXPECT_TRUE(agent->ShouldAutoupgradeMixedContent());
}

}  // namespace content_settings
