// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

using blink::PermissionType;
using blink::mojom::PermissionsPolicyFeature;
using blink::mojom::PermissionStatus;

namespace permissions {
namespace {

class ScopedPartitionedOriginBrowserClient
    : public content::ContentBrowserClient {
 public:
  explicit ScopedPartitionedOriginBrowserClient(const GURL& app_origin)
      : app_origin_(url::Origin::Create(app_origin)) {
    old_client_ = content::SetBrowserClientForTesting(this);
  }

  ~ScopedPartitionedOriginBrowserClient() override {
    content::SetBrowserClientForTesting(old_client_);
  }

  content::StoragePartitionConfig GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override {
    if (url::Origin::Create(site) == app_origin_) {
      return content::StoragePartitionConfig::Create(
          browser_context, "test_partition", /*partition_name=*/std::string(),
          /*in_memory=*/false);
    }
    return content::StoragePartitionConfig::CreateDefault(browser_context);
  }

 private:
  url::Origin app_origin_;
  raw_ptr<content::ContentBrowserClient> old_client_;
};

#if BUILDFLAG(IS_ANDROID)
// See https://crbug.com/904883.
auto GetDefaultProtectedMediaIdentifierPermissionStatus() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
                 base::android::SDK_VERSION_MARSHMALLOW
             ? PermissionStatus::GRANTED
             : PermissionStatus::ASK;
}

auto GetDefaultProtectedMediaIdentifierContentSetting() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
                 base::android::SDK_VERSION_MARSHMALLOW
             ? CONTENT_SETTING_ALLOW
             : CONTENT_SETTING_ASK;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

class PermissionManagerTest : public content::RenderViewHostTestHarness {
 public:
  void OnPermissionChange(PermissionStatus permission) {
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
    callback_called_ = true;
    callback_count_++;
    callback_result_ = permission;
  }

 protected:
  PermissionManagerTest()
      : url_("https://example.com"), other_url_("https://foo.com") {}

  PermissionManager* GetPermissionManager() {
    return static_cast<PermissionManager*>(
        browser_context_->GetPermissionControllerDelegate());
  }

  HostContentSettingsMap* GetHostContentSettingsMap() {
    return PermissionsClient::Get()->GetSettingsMap(browser_context_.get());
  }

  void CheckPermissionStatus(PermissionType type, PermissionStatus expected) {
    EXPECT_EQ(expected, GetPermissionManager()->GetPermissionStatus(
                            type, url_.DeprecatedGetOriginAsURL(),
                            url_.DeprecatedGetOriginAsURL()));
  }

  void CheckPermissionResult(ContentSettingsType type,
                             ContentSetting expected_status,
                             PermissionStatusSource expected_status_source) {
    PermissionResult result =
        GetPermissionManager()->GetPermissionStatusDeprecated(
            type, url_.DeprecatedGetOriginAsURL(),
            url_.DeprecatedGetOriginAsURL());
    EXPECT_EQ(expected_status, result.content_setting);
    EXPECT_EQ(expected_status_source, result.source);
  }

  void SetPermission(ContentSettingsType type, ContentSetting value) {
    SetPermission(url_, type, value);
  }

  void SetPermission(const GURL& origin,
                     ContentSettingsType type,
                     ContentSetting value) {
    GetHostContentSettingsMap()->SetContentSettingDefaultScope(origin, origin,
                                                               type, value);
  }

  void RequestPermissionFromCurrentDocument(PermissionType type,
                                            content::RenderFrameHost* rfh) {
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    GetPermissionManager()->RequestPermissionsFromCurrentDocument(
        std::vector(1, type), rfh, true,
        base::BindOnce(
            [](base::OnceCallback<void(blink::mojom::PermissionStatus)>
                   callback,
               const std::vector<blink::mojom::PermissionStatus>& state) {
              DCHECK_EQ(state.size(), 1U);
              std::move(callback).Run(state[0]);
            },
            base::BindOnce(&PermissionManagerTest::OnPermissionChange,
                           base::Unretained(this))));
    loop.Run();
  }

  void RequestPermissionFromCurrentDocumentNonBlocking(
      PermissionType type,
      content::RenderFrameHost* rfh) {
    GetPermissionManager()->RequestPermissionsFromCurrentDocument(
        std::vector(1, type), rfh, true,
        base::BindOnce(
            [](base::OnceCallback<void(blink::mojom::PermissionStatus)>
                   callback,
               const std::vector<blink::mojom::PermissionStatus>& state) {
              DCHECK_EQ(state.size(), 1U);
              std::move(callback).Run(state[0]);
            },
            base::BindOnce(&PermissionManagerTest::OnPermissionChange,
                           base::Unretained(this))));
  }

  PermissionStatus GetPermissionStatusForCurrentDocument(
      PermissionType permission,
      content::RenderFrameHost* render_frame_host) {
    return GetPermissionManager()->GetPermissionStatusForCurrentDocument(
        permission, render_frame_host);
  }

  PermissionStatus GetPermissionStatusForWorker(
      PermissionType permission,
      content::RenderProcessHost* render_process_host,
      const GURL& worker_origin) {
    return GetPermissionManager()->GetPermissionStatusForWorker(
        permission, render_process_host, worker_origin);
  }

  content::PermissionControllerDelegate::SubscriptionId
  SubscribePermissionStatusChange(
      PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(PermissionStatus)> callback) {
    return GetPermissionManager()->SubscribePermissionStatusChange(
        permission, render_process_host, render_frame_host, requesting_origin,
        std::move(callback));
  }

  void UnsubscribePermissionStatusChange(
      content::PermissionControllerDelegate::SubscriptionId subscription_id) {
    GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
  }

  bool IsPermissionOverridableByDevTools(
      PermissionType permission,
      const absl::optional<url::Origin>& origin) {
    return GetPermissionManager()->IsPermissionOverridableByDevTools(permission,
                                                                     origin);
  }

  void ResetPermission(PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) {
    GetPermissionManager()->ResetPermission(permission, requesting_origin,
                                            embedding_origin);
  }

  const GURL& url() const { return url_; }

  const GURL& other_url() const { return other_url_; }

  bool callback_called() const { return callback_called_; }

  int callback_count() const { return callback_count_; }

  PermissionStatus callback_result() const { return callback_result_; }

  void Reset() {
    callback_called_ = false;
    callback_count_ = 0;
    callback_result_ = PermissionStatus::ASK;
  }

  bool PendingRequestsEmpty() {
    return GetPermissionManager()->pending_requests_.IsEmpty();
  }

  // The header policy should only be set once on page load, so we refresh the
  // page to simulate that.
  void RefreshPageAndSetHeaderPolicy(content::RenderFrameHost** rfh,
                                     PermissionsPolicyFeature feature,
                                     const std::vector<std::string>& origins) {
    content::RenderFrameHost* current = *rfh;
    auto navigation = content::NavigationSimulator::CreateRendererInitiated(
        current->GetLastCommittedURL(), current);
    std::vector<url::Origin> parsed_origins;
    for (const std::string& origin : origins)
      parsed_origins.push_back(url::Origin::Create(GURL(origin)));
    navigation->SetPermissionsPolicyHeader(
        {{feature, parsed_origins, false, false}});
    navigation->Commit();
    *rfh = navigation->GetFinalRenderFrameHost();
  }

  content::RenderFrameHost* AddChildRFH(
      content::RenderFrameHost* parent,
      const GURL& origin,
      PermissionsPolicyFeature feature = PermissionsPolicyFeature::kNotFound) {
    blink::ParsedPermissionsPolicy frame_policy = {};
    if (feature != PermissionsPolicyFeature::kNotFound) {
      frame_policy.push_back(
          {feature, std::vector<url::Origin>{url::Origin::Create(origin)},
           false, false});
    }
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChildWithPolicy(
            "", frame_policy);
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, origin);
    return result;
  }

 private:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    browser_context_->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(browser_context_.get()));
    NavigateAndCommit(url());
  }

  void TearDown() override {
    GetPermissionManager()->Shutdown();
    browser_context_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

  const GURL url_;
  const GURL other_url_;
  bool callback_called_ = false;
  int callback_count_ = 0;
  PermissionStatus callback_result_ = PermissionStatus::ASK;
  base::OnceClosure quit_closure_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  TestPermissionsClient client_;
};

TEST_F(PermissionManagerTest, GetPermissionStatusDefault) {
  CheckPermissionStatus(PermissionType::MIDI_SYSEX, PermissionStatus::ASK);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS, PermissionStatus::ASK);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::ASK);
#if BUILDFLAG(IS_ANDROID)
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        GetDefaultProtectedMediaIdentifierPermissionStatus());
  CheckPermissionStatus(PermissionType::WINDOW_PLACEMENT,
                        PermissionStatus::DENIED);
#else
  CheckPermissionStatus(PermissionType::WINDOW_PLACEMENT,
                        PermissionStatus::ASK);
#endif
}

TEST_F(PermissionManagerTest, GetPermissionStatusAfterSet) {
  SetPermission(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  SetPermission(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS,
                        PermissionStatus::GRANTED);

  SetPermission(ContentSettingsType::MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::MIDI_SYSEX, PermissionStatus::GRANTED);

#if BUILDFLAG(IS_ANDROID)
  SetPermission(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        PermissionStatus::GRANTED);

  SetPermission(ContentSettingsType::WINDOW_PLACEMENT, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::WINDOW_PLACEMENT,
                        PermissionStatus::DENIED);
#else
  SetPermission(ContentSettingsType::WINDOW_PLACEMENT, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::WINDOW_PLACEMENT,
                        PermissionStatus::GRANTED);
#endif
}

TEST_F(PermissionManagerTest, CheckPermissionResultDefault) {
  CheckPermissionResult(ContentSettingsType::MIDI_SYSEX, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
#if BUILDFLAG(IS_ANDROID)
  CheckPermissionResult(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                        GetDefaultProtectedMediaIdentifierContentSetting(),
                        PermissionStatusSource::UNSPECIFIED);
#endif
}

TEST_F(PermissionManagerTest, CheckPermissionResultAfterSet) {
  SetPermission(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

  SetPermission(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

  SetPermission(ContentSettingsType::MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(ContentSettingsType::MIDI_SYSEX, CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

#if BUILDFLAG(IS_ANDROID)
  SetPermission(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionResult(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);
#endif
}

TEST_F(PermissionManagerTest, SubscriptionDestroyedCleanlyWithoutUnsubscribe) {
  // Test that the PermissionManager shuts down cleanly with subscriptions that
  // haven't been removed, crbug.com/720071.
  SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, /*render_process_host=*/nullptr, main_rfh(),
      url(),
      base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                          base::Unretained(this)));
}

TEST_F(PermissionManagerTest, SubscribeUnsubscribeAfterShutdown) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  // Simulate Keyed Services shutdown pass. Note: Shutdown will be called second
  // time during browser_context destruction. This is ok for now: Shutdown is
  // reenterant.
  GetPermissionManager()->Shutdown();

  UnsubscribePermissionStatusChange(subscription_id);

  // Check that subscribe/unsubscribe after shutdown don't crash.
  content::PermissionControllerDelegate::SubscriptionId subscription2_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  UnsubscribePermissionStatusChange(subscription2_id);
}

TEST_F(PermissionManagerTest, SameTypeChangeNotifies) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, DifferentTypeChangeDoesNotNotify) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ChangeAfterUnsubscribeDoesNotNotify) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  UnsubscribePermissionStatusChange(subscription_id);

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());
}

TEST_F(PermissionManagerTest,
       ChangeAfterUnsubscribeOnlyNotifiesActiveSubscribers) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, /*render_process_host=*/nullptr, main_rfh(),
      url(),
      base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                          base::Unretained(this)));

  UnsubscribePermissionStatusChange(subscription_id);

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(callback_count(), 1);
}

TEST_F(PermissionManagerTest, DifferentPrimaryUrlDoesNotNotify) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      other_url(), url(), ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, DifferentSecondaryUrlDoesNotNotify) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::STORAGE_ACCESS_GRANT, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), other_url(), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, WildCardPatternNotifies) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ClearSettingsNotifies) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->ClearSettingsForOneType(
      ContentSettingsType::GEOLOCATION);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, NewValueCorrectlyPassed) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ChangeWithoutPermissionChangeDoesNotNotify) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ChangesBackAndForth) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ASK);

  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  Reset();

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ChangesBackAndForthWorker) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ASK);

  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, process(), /*render_frame_host=*/nullptr,
          url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  Reset();

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, SubscribeMIDIPermission) {
  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::MIDI, /*render_process_host=*/nullptr, main_rfh(),
          url(),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));

  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::ASK);
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, PermissionIgnoredCleanup) {
  content::WebContents* contents = web_contents();
  PermissionRequestManager::CreateForWebContents(contents);
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(contents);
  auto prompt_factory = std::make_unique<MockPermissionPromptFactory>(manager);

  NavigateAndCommit(url());

  RequestPermissionFromCurrentDocumentNonBlocking(PermissionType::GEOLOCATION,
                                                  main_rfh());

  EXPECT_FALSE(PendingRequestsEmpty());

  NavigateAndCommit(GURL("https://foobar.com"));

  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(PendingRequestsEmpty());
}

// Check PermissionResult shows requests denied due to insecure
// origins.
TEST_F(PermissionManagerTest, InsecureOrigin) {
  GURL insecure_frame("http://www.example.com/geolocation");
  NavigateAndCommit(insecure_frame);

  PermissionResult result =
      GetPermissionManager()->GetPermissionStatusForCurrentDocument(
          ContentSettingsType::GEOLOCATION,
          web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::INSECURE_ORIGIN, result.source);

  GURL secure_frame("https://www.example.com/geolocation");
  NavigateAndCommit(secure_frame);

  result = GetPermissionManager()->GetPermissionStatusForCurrentDocument(
      ContentSettingsType::GEOLOCATION, web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
}

TEST_F(PermissionManagerTest, InsecureOriginIsNotOverridable) {
  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://example.com/geolocation"));
  const url::Origin kSecureOrigin =
      url::Origin::Create(GURL("https://example.com/geolocation"));
  EXPECT_FALSE(IsPermissionOverridableByDevTools(PermissionType::GEOLOCATION,
                                                 kInsecureOrigin));
  EXPECT_TRUE(IsPermissionOverridableByDevTools(PermissionType::GEOLOCATION,
                                                kSecureOrigin));
}

TEST_F(PermissionManagerTest, MissingContextIsNotOverridable) {
  // Permissions that are not implemented should be denied overridability.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(IsPermissionOverridableByDevTools(
      PermissionType::PROTECTED_MEDIA_IDENTIFIER,
      url::Origin::Create(GURL("http://localhost"))));
#endif
  EXPECT_TRUE(IsPermissionOverridableByDevTools(
      PermissionType::MIDI_SYSEX,
      url::Origin::Create(GURL("http://localhost"))));
}

TEST_F(PermissionManagerTest, KillSwitchOnIsNotOverridable) {
  const url::Origin kLocalHost = url::Origin::Create(GURL("http://localhost"));
  EXPECT_TRUE(IsPermissionOverridableByDevTools(PermissionType::GEOLOCATION,
                                                kLocalHost));

  // Turn on kill switch for GEOLOCATION.
  std::map<std::string, std::string> params;
  params[PermissionUtil::GetPermissionString(
      ContentSettingsType::GEOLOCATION)] =
      PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  base::AssociateFieldTrialParams(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy, "TestGroup",
      params);
  base::FieldTrialList::CreateFieldTrial(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy, "TestGroup");

  EXPECT_FALSE(IsPermissionOverridableByDevTools(PermissionType::GEOLOCATION,
                                                 kLocalHost));
}

TEST_F(PermissionManagerTest, ResetPermission) {
#if BUILDFLAG(IS_ANDROID)
  CheckPermissionStatus(PermissionType::NOTIFICATIONS, PermissionStatus::ASK);
  SetPermission(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS,
                        PermissionStatus::GRANTED);

  ResetPermission(PermissionType::NOTIFICATIONS, url(), url());

  CheckPermissionStatus(PermissionType::NOTIFICATIONS, PermissionStatus::ASK);
#else
  const char* kOrigin1 = "https://example.com";

  NavigateAndCommit(GURL(kOrigin1));
  content::RenderFrameHost* rfh = main_rfh();

  EXPECT_EQ(PermissionStatus::ASK, GetPermissionStatusForCurrentDocument(
                                       PermissionType::NOTIFICATIONS, rfh));

  PermissionRequestManager::CreateForWebContents(web_contents());
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  auto prompt_factory = std::make_unique<MockPermissionPromptFactory>(manager);
  prompt_factory->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  prompt_factory->DocumentOnLoadCompletedInPrimaryMainFrame();

  RequestPermissionFromCurrentDocument(PermissionType::NOTIFICATIONS, rfh);

  EXPECT_EQ(PermissionStatus::GRANTED, GetPermissionStatusForCurrentDocument(
                                           PermissionType::NOTIFICATIONS, rfh));

  ResetPermission(PermissionType::NOTIFICATIONS, GURL(kOrigin1),
                  GURL(kOrigin1));

  EXPECT_EQ(PermissionStatus::ASK, GetPermissionStatusForCurrentDocument(
                                       PermissionType::NOTIFICATIONS, rfh));
#endif
}

TEST_F(PermissionManagerTest, GetPermissionStatusDelegation) {
  const char* kOrigin1 = "https://example.com";
  const char* kOrigin2 = "https://google.com";

  NavigateAndCommit(GURL(kOrigin1));
  content::RenderFrameHost* parent = main_rfh();

  content::RenderFrameHost* child = AddChildRFH(parent, GURL(kOrigin2));

  // By default the parent should be able to request access, but not the child.
  EXPECT_EQ(PermissionStatus::ASK, GetPermissionStatusForCurrentDocument(
                                       PermissionType::GEOLOCATION, parent));
  EXPECT_EQ(PermissionStatus::DENIED, GetPermissionStatusForCurrentDocument(
                                          PermissionType::GEOLOCATION, child));

  // Enabling geolocation by FP should allow the child to request access also.
  child = AddChildRFH(parent, GURL(kOrigin2),
                      PermissionsPolicyFeature::kGeolocation);

  EXPECT_EQ(PermissionStatus::ASK, GetPermissionStatusForCurrentDocument(
                                       PermissionType::GEOLOCATION, child));

  // When the child requests location a prompt should be displayed for the
  // parent.
  PermissionRequestManager::CreateForWebContents(web_contents());
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  auto prompt_factory = std::make_unique<MockPermissionPromptFactory>(manager);
  prompt_factory->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  prompt_factory->DocumentOnLoadCompletedInPrimaryMainFrame();

  RequestPermissionFromCurrentDocument(PermissionType::GEOLOCATION, child);

  EXPECT_TRUE(prompt_factory->RequestOriginSeen(GURL(kOrigin1)));

  // Now the child frame should have location, as well as the parent frame.
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusForCurrentDocument(PermissionType::GEOLOCATION,
                                                  parent));
  EXPECT_EQ(PermissionStatus::GRANTED, GetPermissionStatusForCurrentDocument(
                                           PermissionType::GEOLOCATION, child));

  // Revoking access from the parent should cause the child not to have access
  // either.
  ResetPermission(PermissionType::GEOLOCATION, GURL(kOrigin1), GURL(kOrigin1));
  EXPECT_EQ(PermissionStatus::ASK, GetPermissionStatusForCurrentDocument(
                                       PermissionType::GEOLOCATION, parent));
  EXPECT_EQ(PermissionStatus::ASK, GetPermissionStatusForCurrentDocument(
                                       PermissionType::GEOLOCATION, child));

  // If the parent changes its policy, the child should be blocked.
  RefreshPageAndSetHeaderPolicy(&parent, PermissionsPolicyFeature::kGeolocation,
                                {kOrigin1});
  child = AddChildRFH(parent, GURL(kOrigin2));

  EXPECT_EQ(PermissionStatus::ASK, GetPermissionStatusForCurrentDocument(
                                       PermissionType::GEOLOCATION, parent));
  EXPECT_EQ(PermissionStatus::DENIED, GetPermissionStatusForCurrentDocument(
                                          PermissionType::GEOLOCATION, child));

  prompt_factory.reset();
}

TEST_F(PermissionManagerTest, SubscribeWithPermissionDelegation) {
  const char* kOrigin1 = "https://example.com";
  const char* kOrigin2 = "https://google.com";

  NavigateAndCommit(GURL(kOrigin1));
  content::RenderFrameHost* parent = main_rfh();
  content::RenderFrameHost* child = AddChildRFH(parent, GURL(kOrigin2));

  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr, child,
          GURL(kOrigin2),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));
  EXPECT_FALSE(callback_called());

  // Location should be blocked for the child because it's not delegated.
  EXPECT_EQ(PermissionStatus::DENIED, GetPermissionStatusForCurrentDocument(
                                          PermissionType::GEOLOCATION, child));

  // Allow access for the top level origin.
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  // The child's permission should still be block and no callback should be run.
  EXPECT_EQ(PermissionStatus::DENIED, GetPermissionStatusForCurrentDocument(
                                          PermissionType::GEOLOCATION, child));

  EXPECT_FALSE(callback_called());

  // Enabling geolocation by FP should allow the child to request access also.
  child = AddChildRFH(parent, GURL(kOrigin2),
                      PermissionsPolicyFeature::kGeolocation);

  EXPECT_EQ(PermissionStatus::GRANTED, GetPermissionStatusForCurrentDocument(
                                           PermissionType::GEOLOCATION, child));

  subscription_id = SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, /*render_process_host=*/nullptr, child,
      GURL(kOrigin2),
      base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                          base::Unretained(this)));
  EXPECT_FALSE(callback_called());

  // Blocking access to the parent should trigger the callback to be run for the
  // child also.
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  EXPECT_EQ(PermissionStatus::DENIED, GetPermissionStatusForCurrentDocument(
                                          PermissionType::GEOLOCATION, child));

  UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, SubscribeUnsubscribeAndResubscribe) {
  const char* kOrigin1 = "https://example.com";
  NavigateAndCommit(GURL(kOrigin1));

  content::PermissionControllerDelegate::SubscriptionId subscription_id =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), GURL(kOrigin1),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));
  EXPECT_EQ(callback_count(), 0);

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(callback_count(), 1);
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  UnsubscribePermissionStatusChange(subscription_id);

  // ensure no callbacks are received when unsubscribed.
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK);
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(callback_count(), 1);

  content::PermissionControllerDelegate::SubscriptionId subscription_id_2 =
      SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
          main_rfh(), GURL(kOrigin1),
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));
  EXPECT_EQ(callback_count(), 1);

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK);

  EXPECT_EQ(callback_count(), 2);
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  UnsubscribePermissionStatusChange(subscription_id_2);
}

TEST_F(PermissionManagerTest, GetCanonicalOrigin) {
  GURL requesting("https://requesting.example.com");
  GURL embedding("https://embedding.example.com");

  EXPECT_EQ(embedding,
            GetPermissionManager()->GetCanonicalOrigin(
                ContentSettingsType::COOKIES, requesting, embedding));
  EXPECT_EQ(requesting,
            GetPermissionManager()->GetCanonicalOrigin(
                ContentSettingsType::NOTIFICATIONS, requesting, embedding));
  EXPECT_EQ(requesting,
            GetPermissionManager()->GetCanonicalOrigin(
                ContentSettingsType::STORAGE_ACCESS, requesting, embedding));
}

TEST_F(PermissionManagerTest, RequestPermissionInDifferentStoragePartition) {
  const GURL kOrigin("https://example.com");
  const GURL kOrigin2("https://example2.com");
  const GURL kPartitionedOrigin("https://partitioned.com");
  ScopedPartitionedOriginBrowserClient browser_client(kPartitionedOrigin);

  SetPermission(kOrigin, ContentSettingsType::GEOLOCATION,
                ContentSetting::CONTENT_SETTING_ALLOW);

  SetPermission(kOrigin2, ContentSettingsType::GEOLOCATION,
                ContentSetting::CONTENT_SETTING_BLOCK);
  SetPermission(kOrigin2, ContentSettingsType::NOTIFICATIONS,
                ContentSetting::CONTENT_SETTING_ALLOW);

  SetPermission(kPartitionedOrigin, ContentSettingsType::GEOLOCATION,
                ContentSetting::CONTENT_SETTING_BLOCK);
  SetPermission(kPartitionedOrigin, ContentSettingsType::NOTIFICATIONS,
                ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateAndCommit(kOrigin);
  content::RenderFrameHost* parent = main_rfh();

  content::RenderFrameHost* child =
      AddChildRFH(parent, kOrigin2, PermissionsPolicyFeature::kGeolocation);
  content::RenderFrameHost* partitioned_child = AddChildRFH(
      parent, kPartitionedOrigin, PermissionsPolicyFeature::kGeolocation);

  // The parent should have geolocation access which is delegated to child and
  // partitioned_child.
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusForCurrentDocument(PermissionType::GEOLOCATION,
                                                  parent));
  EXPECT_EQ(PermissionStatus::GRANTED, GetPermissionStatusForCurrentDocument(
                                           PermissionType::GEOLOCATION, child));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusForCurrentDocument(PermissionType::GEOLOCATION,
                                                  partitioned_child));

  // The parent should not have notification permission.
  EXPECT_EQ(PermissionStatus::ASK, GetPermissionStatusForCurrentDocument(
                                       PermissionType::NOTIFICATIONS, parent));
  EXPECT_EQ(PermissionStatus::ASK,
            GetPermissionStatusForWorker(
                PermissionType::NOTIFICATIONS, parent->GetProcess(),
                parent->GetLastCommittedOrigin().GetURL()));

  // The non-partitioned child should have notification permission.
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusForCurrentDocument(PermissionType::NOTIFICATIONS,
                                                  child));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusForWorker(
                PermissionType::NOTIFICATIONS, child->GetProcess(),
                child->GetLastCommittedOrigin().GetURL()));

  // The partitioned child should not have notification permission because it
  // belongs to a different StoragePartition, even though its origin would have
  // permission if loaded in a main frame.
  EXPECT_EQ(PermissionStatus::DENIED,
            GetPermissionStatusForCurrentDocument(PermissionType::NOTIFICATIONS,
                                                  partitioned_child));
  EXPECT_EQ(PermissionStatus::DENIED,
            GetPermissionStatusForWorker(
                PermissionType::NOTIFICATIONS, partitioned_child->GetProcess(),
                partitioned_child->GetLastCommittedOrigin().GetURL()));
}

}  // namespace permissions
