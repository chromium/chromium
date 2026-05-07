// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/prefs/pref_service.h"
#endif

// This file contains tests for PermissionContextBase,
// ContentSettingPermissionContextBase and GeolocationPermissionContext.

namespace permissions {

// We can't use content_settings::GeolocationContentSettingsType() because this
// must be constexpr.
constexpr ContentSettingsType kGeolocationContentSettingsType =
#if BUILDFLAG(IS_ANDROID)
    ContentSettingsType::GEOLOCATION_WITH_OPTIONS;
#else
    ContentSettingsType::GEOLOCATION;
#endif

using PermissionStatus = blink::mojom::PermissionStatus;

const char* const kPermissionsKillSwitchFieldStudy =
    ContentSettingPermissionContextBase::kPermissionsKillSwitchFieldStudy;
const char* const kPermissionsKillSwitchBlockedValue =
    ContentSettingPermissionContextBase::kPermissionsKillSwitchBlockedValue;
const char kPermissionsKillSwitchTestGroup[] = "TestGroup";
constexpr int kDefaultDismissalsBeforeBlock = 3;

PermissionRequestManager::AutoResponseType ContentSettingToDecision(
    ContentSetting content_setting) {
  using AutoResponseType = PermissionRequestManager::AutoResponseType;
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      return AutoResponseType::ACCEPT_ALL;
    case CONTENT_SETTING_BLOCK:
      return AutoResponseType::DENY_ALL;
    case CONTENT_SETTING_ASK:
      return AutoResponseType::DISMISS;
    default:
      break;
  }
  NOTREACHED();
}

class TestGeolocationPermissionContextDelegate
    : public GeolocationPermissionContext::Delegate {
 public:
  explicit TestGeolocationPermissionContextDelegate(
      content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_ANDROID)
    GeolocationPermissionContextAndroid::RegisterProfilePrefs(
        prefs_.registry());
#endif
  }

  bool DecidePermission(const PermissionRequestData& request_data,
                        BrowserPermissionCallback* callback,
                        GeolocationPermissionContext* context) override {
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  bool IsInteractable(content::WebContents* web_contents) override {
    return true;
  }

  PrefService* GetPrefs(content::BrowserContext* browser_context) override {
    return &prefs_;
  }

  bool IsRequestingOriginDSE(content::BrowserContext* browser_context,
                             const GURL& requesting_origin) override {
    return false;
  }
#endif

 private:
  TestingPrefServiceSimple prefs_;
};

// A templated class which can extend either
// ContentSettingPermissionContextBase or GeolocationPermissionContext.
template <typename T>
class TestPermissionContext : public T {
 public:
  TestPermissionContext(content::BrowserContext* browser_context,
                        const ContentSettingsType content_settings_type
#if BUILDFLAG(IS_ANDROID)
                        ,
                        bool enabled_app_level_notification_permission
#endif  // BUILDFLAG(IS_ANDROID)
                        )
    requires(std::is_same_v<T, ContentSettingPermissionContextBase>)
      : T(browser_context,
          content_settings_type,
          network::mojom::PermissionsPolicyFeature::kNotFound) {
#if BUILDFLAG(IS_ANDROID)
    if (content_settings_type == ContentSettingsType::NOTIFICATIONS) {
      this->enabled_app_level_notification_permission_for_testing_ =
          enabled_app_level_notification_permission;
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }
  TestPermissionContext(
      content::BrowserContext* browser_context,
      std::unique_ptr<GeolocationPermissionContext::Delegate> delegate)
    requires(std::is_same_v<T, GeolocationPermissionContext>)
      : GeolocationPermissionContext(browser_context, std::move(delegate)) {}

  TestPermissionContext(const TestPermissionContext&) = delete;
  TestPermissionContext& operator=(const TestPermissionContext&) = delete;

  ~TestPermissionContext() override = default;

  const std::vector<PermissionStatus>& permission_statuses() const {
    return permission_statuses_;
  }

  bool tab_context_updated() const { return tab_context_updated_; }

  // Once a decision for the requested permission has been made, run the
  // callback.
  void TrackPermissionDecision(content::PermissionResult permission_result) {
    permission_statuses_.push_back(permission_result.status);
    // Null check required here as the quit_closure_ can also be run and reset
    // first from within DecidePermission.
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  PermissionSetting GetPermissionSettingFromMap(const GURL& url_a,
                                                const GURL& url_b) {
    auto* map = PermissionsClient::Get()->GetSettingsMap(T::browser_context());
    return map->GetPermissionSetting(url_a.DeprecatedGetOriginAsURL(),
                                     url_b.DeprecatedGetOriginAsURL(),
                                     T::content_settings_type());
  }

  void RequestPermission(std::unique_ptr<PermissionRequestData> request_data,
                         BrowserPermissionCallback callback) override {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    T::RequestPermission(std::move(request_data), std::move(callback));
    run_loop.Run();
  }

  void DecidePermission(std::unique_ptr<PermissionRequestData> request_data,
                        BrowserPermissionCallback callback) override {
    T::DecidePermission(std::move(request_data), std::move(callback));
    if (respond_permission_) {
      std::move(respond_permission_).Run();
    } else {
      // Stop the run loop from spinning indefinitely if no response callback
      // has been set, as is the case with TestParallelRequests.
      std::move(quit_closure_).Run();
    }
  }

  // Set the callback to run if the permission is being responded to in the
  // test. This is left empty where no response is needed, such as in parallel
  // requests, invalid origin, and killswitch.
  void SetRespondPermissionCallback(base::OnceClosure callback) {
    respond_permission_ = std::move(callback);
  }

  void SetUsesAutomaticEmbargo(bool value) { uses_automatic_embargo_ = value; }

 protected:
  void UpdateTabContext(const PermissionRequestData& request_data,
                        bool allowed) override {
    tab_context_updated_ = true;
  }

  bool IsRestrictedToSecureOrigins() const override { return false; }

  bool UsesAutomaticEmbargo() const override { return uses_automatic_embargo_; }

 private:
  std::vector<PermissionStatus> permission_statuses_;
  bool tab_context_updated_ = false;
  base::OnceClosure quit_closure_;
  // Callback for responding to a permission once the request has been completed
  // (valid URL, kill switch disabled)
  base::OnceClosure respond_permission_;
  bool uses_automatic_embargo_ = true;
};

template <typename T>
class TestSecureOriginRestrictedPermissionContext
    : public TestPermissionContext<T> {
 public:
  template <typename... Args>
  explicit TestSecureOriginRestrictedPermissionContext(Args&&... args)
      : TestPermissionContext<T>(std::forward<Args>(args)...) {}

  TestSecureOriginRestrictedPermissionContext(
      const TestSecureOriginRestrictedPermissionContext&) = delete;
  TestSecureOriginRestrictedPermissionContext& operator=(
      const TestSecureOriginRestrictedPermissionContext&) = delete;

 protected:
  bool IsRestrictedToSecureOrigins() const override { return true; }
};

class PermissionContextBaseTestClient : public TestPermissionsClient {
 public:
  bool CanBypassEmbeddingOriginCheck(const GURL& requesting_origin,
                                     const GURL& embedding_origin) override {
    return requesting_origin.SchemeIs("chrome-extension");
  }

  bool IsActorOperatingOnWebContents(
      content::WebContents* web_contents) const override {
    return is_actor_acting_on_web_contents_;
  }

  bool is_actor_acting_on_web_contents_ = false;
};

class PermissionContextBaseTests : public content::RenderViewHostTestHarness {
 public:
  PermissionContextBaseTests(const PermissionContextBaseTests&) = delete;
  PermissionContextBaseTests& operator=(const PermissionContextBaseTests&) =
      delete;

 protected:
  PermissionContextBaseTests() {
#if BUILDFLAG(IS_ANDROID)
    scoped_feature_list_.InitAndEnableFeature(
        content_settings::features::kApproximateGeolocationPermission);
#else
    scoped_feature_list_.InitAndDisableFeature(
        content_settings::features::kApproximateGeolocationPermission);
#endif
  }
  ~PermissionContextBaseTests() override = default;

  template <ContentSettingsType content_settings_type>
  auto CreateTestPermissionContext(
#if BUILDFLAG(IS_ANDROID)
      bool enable_app_level_notification_permission = true
#endif
  ) {
    return TestPermissionContext<ContentSettingPermissionContextBase>(
        browser_context(), content_settings_type
#if BUILDFLAG(IS_ANDROID)
        ,
        enable_app_level_notification_permission
#endif
    );
  }
  template <>
  auto
  CreateTestPermissionContext<ContentSettingsType::GEOLOCATION_WITH_OPTIONS>(
#if BUILDFLAG(IS_ANDROID)
      bool enable_app_level_notification_permission
#endif
  ) {
    return TestPermissionContext<GeolocationPermissionContext>(
        browser_context(),
        std::make_unique<TestGeolocationPermissionContextDelegate>(
            browser_context()));
  }

  template <ContentSettingsType content_settings_type>
  auto CreateTestSecureOriginRestrictedPermissionContext() {
    return TestSecureOriginRestrictedPermissionContext<
        ContentSettingPermissionContextBase>(browser_context(),
                                             content_settings_type);
  }
  template <>
  auto CreateTestSecureOriginRestrictedPermissionContext<
      ContentSettingsType::GEOLOCATION_WITH_OPTIONS>() {
    return TestSecureOriginRestrictedPermissionContext<
        GeolocationPermissionContext>(
        browser_context(),
        std::make_unique<TestGeolocationPermissionContextDelegate>(
            browser_context()));
  }

  PermissionStatus ContentSettingToPermissionStatus(ContentSetting response) {
    switch (response) {
      case CONTENT_SETTING_DEFAULT:
        return PermissionStatus::ASK;
      case CONTENT_SETTING_ALLOW:
        return PermissionStatus::GRANTED;
      case CONTENT_SETTING_BLOCK:
        return PermissionStatus::DENIED;
      case CONTENT_SETTING_ASK:
        return PermissionStatus::ASK;
      case CONTENT_SETTING_SESSION_ONLY:
      case CONTENT_SETTING_NUM_SETTINGS:
        NOTREACHED();
    }
  }

  // Accept or dismiss the permission prompt.
  void RespondToPermission(PermissionRequestManager::AutoResponseType decision,
                           PromptOptions prompt_options) {
    prompt_factory_->set_response_type(decision);
    prompt_factory_->set_response_prompt_options(prompt_options);
  }

  template <ContentSettingsType content_settings_type>
  void TestAskAndDecide_TestContent(
      blink::mojom::PermissionDescriptorPtr permission_descriptor,
      ContentSetting decision) {
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    ASSERT_EQ(content_settings_type,
              PermissionUtil::PermissionTypeToContentSettingsType(
                  blink::PermissionDescriptorToPermissionType(
                      permission_descriptor)));
    auto permission_context =
        CreateTestPermissionContext<content_settings_type>();
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    const PermissionRequestID id(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
        PermissionRequestID::RequestLocalId());
    PromptOptions prompt_options =
        content_settings_type == ContentSettingsType::GEOLOCATION_WITH_OPTIONS
            ? PromptOptions(GeolocationPromptOptions{
                  .selected_accuracy = GeolocationAccuracy::kPrecise})
            : std::monostate();
    permission_context.SetRespondPermissionCallback(
        base::BindOnce(&PermissionContextBaseTests::RespondToPermission,
                       base::Unretained(this),
                       ContentSettingToDecision(decision), prompt_options));
    permission_context.RequestPermission(
        std::make_unique<PermissionRequestData>(
            std::move(permission_descriptor), id,
            /*user_gesture=*/true, url),
        base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                       base::Unretained(&permission_context)));
    ASSERT_EQ(1u, permission_context.permission_statuses().size());
    EXPECT_EQ(ContentSettingToPermissionStatus(decision),
              permission_context.permission_statuses()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());

    std::string decision_string;
    std::optional<PermissionAction> action;
    if (decision == CONTENT_SETTING_ALLOW) {
      decision_string = "Accepted";
      action = PermissionAction::GRANTED;
    } else if (decision == CONTENT_SETTING_BLOCK) {
      decision_string = "Denied";
      action = PermissionAction::DENIED;
    } else if (decision == CONTENT_SETTING_ASK) {
      decision_string = "Dismissed";
      action = PermissionAction::DISMISSED;
    }

    if (!decision_string.empty()) {
      histograms.ExpectUniqueSample(
          "Permissions.Prompt." + decision_string + ".PriorDismissCount2." +
              PermissionUtil::GetPermissionString(content_settings_type),
          0, 1);
      histograms.ExpectUniqueSample(
          "Permissions.Prompt." + decision_string + ".PriorIgnoreCount2." +
              PermissionUtil::GetPermissionString(content_settings_type),
          0, 1);
#if BUILDFLAG(IS_ANDROID)
      histograms.ExpectUniqueSample(
          "Permissions.Action.WithDisposition.ModalDialog",
          static_cast<int>(action.value()), 1);
#else
      histograms.ExpectUniqueSample(
          "Permissions.Action.WithDisposition.AnchoredBubble",
          static_cast<int>(action.value()), 1);
#endif
    }

    const content_settings::PermissionSettingsInfo* info =
        content_settings::PermissionSettingsRegistry::GetInstance()->Get(
            content_settings_type);
    EXPECT_EQ(info->delegate().ToPermissionSetting(decision),
              permission_context.GetPermissionSettingFromMap(url, url));

    histograms.ExpectUniqueSample(
        "Permissions.AutoBlocker.EmbargoPromptSuppression",
        static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), 1);
    histograms.ExpectUniqueSample(
        "Permissions.AutoBlocker.EmbargoStatus",
        static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), 1);

    if (action.has_value()) {
      auto entries = ukm_recorder.GetEntriesByName("Permission");
      EXPECT_EQ(1u, entries.size());
      auto* entry = entries.front().get();
      ukm_recorder.ExpectEntrySourceHasUrl(entry, url);

      EXPECT_NE(content_settings_uma_util::ContentSettingTypeToHistogramValue(
                    content_settings_type),
                -1);

      EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Source"),
                static_cast<int64_t>(PermissionSourceUI::PROMPT));
      EXPECT_EQ(
          *ukm_recorder.GetEntryMetric(entry, "PermissionType"),
          static_cast<int64_t>(
              content_settings_uma_util::ContentSettingTypeToHistogramValue(
                  content_settings_type)));
      EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Action"),
                static_cast<int64_t>(action.value()));

#if BUILDFLAG(IS_ANDROID)
      EXPECT_EQ(
          *ukm_recorder.GetEntryMetric(entry, "PromptDisposition"),
          static_cast<int64_t>(PermissionPromptDisposition::MODAL_DIALOG));
#else
      EXPECT_EQ(
          *ukm_recorder.GetEntryMetric(entry, "PromptDisposition"),
          static_cast<int64_t>(PermissionPromptDisposition::ANCHORED_BUBBLE));
#endif
    }
  }

  template <ContentSettingsType content_settings_type>
  void DismissMultipleTimesAndExpectBlock(
      const GURL& url,
      blink::mojom::PermissionName permission_name,
      uint32_t iterations) {
    base::HistogramTester histograms;

    blink::mojom::PermissionDescriptorPtr permission_descriptor =
        blink::mojom::PermissionDescriptor::New(permission_name,
                                                /*extension=*/nullptr);
    ASSERT_EQ(content_settings_type,
              PermissionUtil::PermissionTypeToContentSettingsType(
                  blink::PermissionDescriptorToPermissionType(
                      permission_descriptor)));

    // Dismiss |iterations| times. The final dismiss should change the decision
    // from dismiss to block, and hence change the persisted content setting.
    for (uint32_t i = 0; i < iterations; ++i) {
      auto permission_context =
          CreateTestPermissionContext<content_settings_type>();
      const PermissionRequestID id(
          web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
          PermissionRequestID::RequestLocalId());

      permission_context.SetRespondPermissionCallback(
          base::BindOnce(&PermissionContextBaseTests::RespondToPermission,
                         base::Unretained(this),
                         PermissionRequestManager::AutoResponseType::DISMISS,
                         std::monostate()));

      permission_context.RequestPermission(
          std::make_unique<PermissionRequestData>(permission_descriptor.Clone(),
                                                  id,
                                                  /*user_gesture=*/true, url),
          base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                         base::Unretained(&permission_context)));
      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2." +
              PermissionUtil::GetPermissionString(content_settings_type),
          i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2." +
              PermissionUtil::GetPermissionString(content_settings_type),
          i, 1);

      histograms.ExpectTotalCount("Permissions.AutoBlocker.EmbargoStatus",
                                  i + 1);

      content::PermissionResult result = permission_context.GetPermissionStatus(
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  permissions::PermissionUtil::
                      ContentSettingsTypeToPermissionType(
                          permission_context.content_settings_type())),
          nullptr /* render_frame_host */, url, url);

      histograms.ExpectUniqueSample(
          "Permissions.AutoBlocker.EmbargoPromptSuppression",
          static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      if (i < 2) {
        EXPECT_EQ(content::PermissionStatusSource::UNSPECIFIED, result.source);
        EXPECT_EQ(PermissionStatus::ASK, result.status);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      } else {
        EXPECT_EQ(content::PermissionStatusSource::MULTIPLE_DISMISSALS,
                  result.source);
        EXPECT_EQ(PermissionStatus::DENIED, result.status);
        histograms.ExpectBucketCount(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::REPEATED_DISMISSALS), 1);
      }

      ASSERT_EQ(1u, permission_context.permission_statuses().size());
      EXPECT_EQ(ContentSettingToPermissionStatus(CONTENT_SETTING_ASK),
                permission_context.permission_statuses()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
    }

    auto permission_context =
        CreateTestPermissionContext<content_settings_type>();

    const PermissionRequestID id(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
        PermissionRequestID::RequestLocalId());

    permission_context.SetRespondPermissionCallback(
        base::BindOnce(&PermissionContextBaseTests::RespondToPermission,
                       base::Unretained(this),
                       PermissionRequestManager::AutoResponseType::DISMISS,
                       /*prompt_options=*/std::monostate()));

    permission_context.RequestPermission(
        std::make_unique<PermissionRequestData>(
            blink::mojom::PermissionDescriptor::New(permission_name,
                                                    /*extension=*/nullptr),
            id, /*user_gesture=*/true, url),
        base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                       base::Unretained(&permission_context)));

    content::PermissionResult result = permission_context.GetPermissionStatus(
        content::PermissionDescriptorUtil::
            CreatePermissionDescriptorForPermissionType(
                permissions::PermissionUtil::
                    ContentSettingsTypeToPermissionType(
                        permission_context.content_settings_type())),
        nullptr /* render_frame_host */, url, url);
    EXPECT_EQ(PermissionStatus::DENIED, result.status);
    EXPECT_EQ(content::PermissionStatusSource::MULTIPLE_DISMISSALS,
              result.source);
    histograms.ExpectBucketCount(
        "Permissions.AutoBlocker.EmbargoPromptSuppression",
        static_cast<int>(PermissionEmbargoStatus::REPEATED_DISMISSALS), 1);
  }

  void TestBlockOnSeveralDismissals_TestContent() {
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    // Sanity check independence per permission type by checking two of them.
    DismissMultipleTimesAndExpectBlock<kGeolocationContentSettingsType>(
        url, blink::mojom::PermissionName::GEOLOCATION, 3);
    DismissMultipleTimesAndExpectBlock<ContentSettingsType::NOTIFICATIONS>(
        url, blink::mojom::PermissionName::NOTIFICATIONS, 3);
  }

  void TestVariationBlockOnSeveralDismissals_TestContent() {
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    for (uint32_t i = 0; i < kDefaultDismissalsBeforeBlock; ++i) {
      auto permission_context =
          CreateTestPermissionContext<ContentSettingsType::MIDI_SYSEX>();

      const PermissionRequestID id(
          web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
          PermissionRequestID::RequestLocalId(i + 1));
      permission_context.SetRespondPermissionCallback(
          base::BindOnce(&PermissionContextBaseTests::RespondToPermission,
                         base::Unretained(this),
                         PermissionRequestManager::AutoResponseType::DISMISS,
                         /*prompt_options=*/std::monostate()));
      permission_context.RequestPermission(
          std::make_unique<PermissionRequestData>(
              blink::mojom::PermissionDescriptor::New(
                  blink::mojom::PermissionName::MIDI,
                  blink::mojom::PermissionDescriptorExtension::NewMidi(
                      blink::mojom::MidiPermissionDescriptor::New(true))),
              id,
              /*user_gesture=*/true, url),
          base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                         base::Unretained(&permission_context)));

      EXPECT_EQ(1u, permission_context.permission_statuses().size());
      ASSERT_EQ(ContentSettingToPermissionStatus(CONTENT_SETTING_ASK),
                permission_context.permission_statuses()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
      content::PermissionResult result = permission_context.GetPermissionStatus(
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  permissions::PermissionUtil::
                      ContentSettingsTypeToPermissionType(
                          permission_context.content_settings_type())),
          nullptr /* render_frame_host */, url, url);

      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2.MidiSysEx", i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2.MidiSysEx", i, 1);
      histograms.ExpectUniqueSample(
          "Permissions.AutoBlocker.EmbargoPromptSuppression",
          static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      histograms.ExpectTotalCount("Permissions.AutoBlocker.EmbargoStatus",
                                  i + 1);
      if (i < kDefaultDismissalsBeforeBlock - 1) {
        EXPECT_EQ(PermissionStatus::ASK, result.status);
        EXPECT_EQ(content::PermissionStatusSource::UNSPECIFIED, result.source);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      } else {
        EXPECT_EQ(PermissionStatus::DENIED, result.status);
        EXPECT_EQ(content::PermissionStatusSource::MULTIPLE_DISMISSALS,
                  result.source);
        histograms.ExpectBucketCount(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::REPEATED_DISMISSALS), 1);
      }
    }

    // Ensure that we finish in the block state.
    auto permission_context =
        CreateTestPermissionContext<ContentSettingsType::MIDI_SYSEX>();
    content::PermissionResult result = permission_context.GetPermissionStatus(
        content::PermissionDescriptorUtil::
            CreatePermissionDescriptorForPermissionType(
                permissions::PermissionUtil::
                    ContentSettingsTypeToPermissionType(
                        ContentSettingsType::MIDI_SYSEX)),
        nullptr /* render_frame_host */, url, url);
    EXPECT_EQ(PermissionStatus::DENIED, result.status);
    EXPECT_EQ(content::PermissionStatusSource::MULTIPLE_DISMISSALS,
              result.source);
  }

  void TestVariationBlockOnSeveralDismissalsAutomaticEmbargoOff_TestContent() {
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    for (uint32_t i = 0; i < kDefaultDismissalsBeforeBlock; ++i) {
      auto permission_context =
          CreateTestPermissionContext<ContentSettingsType::MIDI_SYSEX>();
      permission_context.SetUsesAutomaticEmbargo(false);

      const PermissionRequestID id(
          web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
          PermissionRequestID::RequestLocalId(i + 1));
      permission_context.SetRespondPermissionCallback(
          base::BindOnce(&PermissionContextBaseTests::RespondToPermission,
                         base::Unretained(this),
                         PermissionRequestManager::AutoResponseType::DISMISS,
                         std::monostate()));
      permission_context.RequestPermission(
          std::make_unique<PermissionRequestData>(
              blink::mojom::PermissionDescriptor::New(
                  blink::mojom::PermissionName::MIDI,
                  blink::mojom::PermissionDescriptorExtension::NewMidi(
                      blink::mojom::MidiPermissionDescriptor::New(true))),
              id,
              /*user_gesture=*/true, url),
          base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                         base::Unretained(&permission_context)));

      EXPECT_EQ(1u, permission_context.permission_statuses().size());
      ASSERT_EQ(ContentSettingToPermissionStatus(CONTENT_SETTING_ASK),
                permission_context.permission_statuses()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
      content::PermissionResult result = permission_context.GetPermissionStatus(
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  permissions::PermissionUtil::
                      ContentSettingsTypeToPermissionType(
                          permission_context.content_settings_type())),
          nullptr /* render_frame_host */, url, url);

      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2.MidiSysEx", i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2.MidiSysEx", 0,
          i + 1);

      histograms.ExpectUniqueSample(
          "Permissions.AutoBlocker.EmbargoPromptSuppression",
          static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      histograms.ExpectTotalCount("Permissions.AutoBlocker.EmbargoStatus",
                                  i + 1);

      EXPECT_EQ(PermissionStatus::ASK, result.status);
      EXPECT_EQ(content::PermissionStatusSource::UNSPECIFIED, result.source);
      histograms.ExpectUniqueSample(
          "Permissions.AutoBlocker.EmbargoStatus",
          static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
    }

    // Ensure that we DO NOT finish in the block state (unlike when automatic
    // embargo is enabled).
    auto permission_context =
        CreateTestPermissionContext<ContentSettingsType::MIDI_SYSEX>();
    permission_context.SetUsesAutomaticEmbargo(false);
    content::PermissionResult result = permission_context.GetPermissionStatus(
        content::PermissionDescriptorUtil::
            CreatePermissionDescriptorForPermissionType(
                permissions::PermissionUtil::
                    ContentSettingsTypeToPermissionType(
                        permission_context.content_settings_type())),
        nullptr /* render_frame_host */, url, url);
    EXPECT_EQ(PermissionStatus::ASK, result.status);
    EXPECT_EQ(content::PermissionStatusSource::UNSPECIFIED, result.source);
  }

  template <ContentSettingsType content_settings_type>
  void TestRequestPermissionInvalidUrl(blink::PermissionType permission_type) {
    base::HistogramTester histograms;
    ASSERT_EQ(
        content_settings_type,
        PermissionUtil::PermissionTypeToContentSettingsType(permission_type));
    auto permission_context =
        CreateTestPermissionContext<content_settings_type>();
    GURL url;
    ASSERT_FALSE(url.is_valid());
    controller().LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                         std::string());

    const PermissionRequestID id(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
        PermissionRequestID::RequestLocalId());
    permission_context.RequestPermission(
        std::make_unique<PermissionRequestData>(
            content::PermissionDescriptorUtil::
                CreatePermissionDescriptorForPermissionType(permission_type),
            id,
            /*user_gesture=*/true, url),
        base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                       base::Unretained(&permission_context)));

    ASSERT_EQ(1u, permission_context.permission_statuses().size());
    EXPECT_EQ(blink::mojom::PermissionStatus::DENIED,
              permission_context.permission_statuses()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());
    const content_settings::PermissionSettingsInfo* info =
        content_settings::PermissionSettingsRegistry::GetInstance()->Get(
            content_settings_type);
    EXPECT_EQ(info->delegate().ToPermissionSetting(CONTENT_SETTING_ASK),
              permission_context.GetPermissionSettingFromMap(url, url));
    histograms.ExpectTotalCount(
        "Permissions.AutoBlocker.EmbargoPromptSuppression", 0);
  }

  template <ContentSettingsType content_settings_type>
  void TestGrantAndRevoke_TestContent(blink::PermissionType permission_type,
                                      ContentSetting expected_default) {
    ASSERT_EQ(
        content_settings_type,
        PermissionUtil::PermissionTypeToContentSettingsType(permission_type));
    auto permission_context =
        CreateTestPermissionContext<content_settings_type>();
    GURL url("https://www.google.com");
    SetUpUrl(url);

    const PermissionRequestID id(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
        PermissionRequestID::RequestLocalId());
    PromptOptions prompt_options =
        content_settings_type == ContentSettingsType::GEOLOCATION_WITH_OPTIONS
            ? PromptOptions(GeolocationPromptOptions{
                  .selected_accuracy = GeolocationAccuracy::kPrecise})
            : std::monostate();
    permission_context.SetRespondPermissionCallback(
        base::BindOnce(&PermissionContextBaseTests::RespondToPermission,
                       base::Unretained(this),
                       PermissionRequestManager::AutoResponseType::ACCEPT_ALL,
                       prompt_options));

    permission_context.RequestPermission(
        std::make_unique<PermissionRequestData>(
            content::PermissionDescriptorUtil::
                CreatePermissionDescriptorForPermissionType(permission_type),
            id,
            /*user_gesture=*/true, url),
        base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                       base::Unretained(&permission_context)));

    const content_settings::PermissionSettingsInfo* info =
        content_settings::PermissionSettingsRegistry::GetInstance()->Get(
            content_settings_type);

    ASSERT_EQ(1u, permission_context.permission_statuses().size());
    EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED,
              permission_context.permission_statuses()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());
    EXPECT_EQ(info->delegate().ToPermissionSetting(CONTENT_SETTING_ALLOW),
              permission_context.GetPermissionSettingFromMap(url, url));

    // Try to reset permission.
    permission_context.ResetPermission(url.DeprecatedGetOriginAsURL(),
                                       url.DeprecatedGetOriginAsURL());
    PermissionSetting setting_after_reset =
        permission_context.GetPermissionSettingFromMap(url, url);
    PermissionSetting default_setting =
        PermissionsClient::Get()
            ->GetSettingsMap(browser_context())
            ->GetDefaultPermissionSetting(content_settings_type, nullptr);
    EXPECT_EQ(default_setting, setting_after_reset);
  }

  template <ContentSettingsType content_settings_type>
  void TestGlobalPermissionsKillSwitch() {
    base::test::ScopedFeatureList scoped_feature_list;
    auto permission_context =
        CreateTestPermissionContext<content_settings_type>();
    scoped_feature_list.Init();

    EXPECT_FALSE(permission_context.IsPermissionKillSwitchOn());
    std::map<std::string, std::string> params;
    params[PermissionUtil::GetPermissionString(content_settings_type)] =
        kPermissionsKillSwitchBlockedValue;
    base::AssociateFieldTrialParams(kPermissionsKillSwitchFieldStudy,
                                    kPermissionsKillSwitchTestGroup, params);
    base::FieldTrialList::CreateFieldTrial(kPermissionsKillSwitchFieldStudy,
                                           kPermissionsKillSwitchTestGroup);
    EXPECT_TRUE(permission_context.IsPermissionKillSwitchOn());
  }

  void TestSecureOriginRestrictedPermissionContextCheck(
      const std::string& requesting_url_spec,
      const std::string& embedding_url_spec,
      bool expect_allowed) {
    GURL requesting_origin(requesting_url_spec);
    GURL embedding_origin(embedding_url_spec);
    auto permission_context = CreateTestSecureOriginRestrictedPermissionContext<
        kGeolocationContentSettingsType>();
    bool result = permission_context.IsPermissionAvailableToOrigins(
        requesting_origin, embedding_origin);
    EXPECT_EQ(expect_allowed, result)
        << "test case (requesting, embedding): (" << requesting_url_spec << ", "
        << embedding_url_spec << ") with secure-origin requirement"
        << " on";

    // With no secure-origin limitation, this check should always return pass.
    auto new_context =
        CreateTestPermissionContext<kGeolocationContentSettingsType>();
    result = new_context.IsPermissionAvailableToOrigins(requesting_origin,
                                                        embedding_origin);
    EXPECT_EQ(true, result)
        << "test case (requesting, embedding): (" << requesting_url_spec << ", "
        << embedding_url_spec << ") with secure-origin requirement"
        << " off";
  }

  // Don't call this more than once in the same test, as it persists data to
  // HostContentSettingsMap.
  void TestParallelRequests(ContentSetting response) {
    auto permission_context =
        CreateTestPermissionContext<kGeolocationContentSettingsType>();
    GURL url("http://www.google.com");
    SetUpUrl(url);

    const PermissionRequestID id1(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
        PermissionRequestID::RequestLocalId(1));
    const PermissionRequestID id2(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
        PermissionRequestID::RequestLocalId(2));

    // Request a permission without setting the callback to DecidePermission.
    permission_context.RequestPermission(
        std::make_unique<PermissionRequestData>(
            content::PermissionDescriptorUtil::
                CreatePermissionDescriptorForPermissionType(
                    blink::PermissionType::GEOLOCATION),
            id1,
            /*user_gesture=*/true, url),
        base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                       base::Unretained(&permission_context)));

    EXPECT_EQ(0u, permission_context.permission_statuses().size());

    PromptOptions prompt_options =
        kGeolocationContentSettingsType ==
                ContentSettingsType::GEOLOCATION_WITH_OPTIONS
            ? PromptOptions(GeolocationPromptOptions{
                  .selected_accuracy = GeolocationAccuracy::kPrecise})
            : std::monostate();
    // Set the callback, and make a second permission request.
    permission_context.SetRespondPermissionCallback(
        base::BindOnce(&PermissionContextBaseTests::RespondToPermission,
                       base::Unretained(this),
                       ContentSettingToDecision(response), prompt_options));
    permission_context.RequestPermission(
        std::make_unique<PermissionRequestData>(
            content::PermissionDescriptorUtil::
                CreatePermissionDescriptorForPermissionType(
                    blink::PermissionType::GEOLOCATION),
            id2,
            /*user_gesture=*/true, url),
        base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                       base::Unretained(&permission_context)));

    ASSERT_EQ(2u, permission_context.permission_statuses().size());
    EXPECT_EQ(ContentSettingToPermissionStatus(response),
              permission_context.permission_statuses()[0]);
    EXPECT_EQ(ContentSettingToPermissionStatus(response),
              permission_context.permission_statuses()[1]);
    EXPECT_TRUE(permission_context.tab_context_updated());

    const content_settings::PermissionSettingsInfo* info =
        content_settings::PermissionSettingsRegistry::GetInstance()->Get(
            kGeolocationContentSettingsType);

    EXPECT_EQ(info->delegate().ToPermissionSetting(response),
              permission_context.GetPermissionSettingFromMap(url, url));
  }

  void TestVirtualURL(const GURL& loaded_url,
                      const GURL& virtual_url,
                      blink::mojom::PermissionStatus want_response,
                      const content::PermissionStatusSource& want_source) {
    auto permission_context =
        CreateTestPermissionContext<kGeolocationContentSettingsType>();

    NavigateAndCommit(loaded_url);
    web_contents()->GetController().GetVisibleEntry()->SetVirtualURL(
        virtual_url);

    content::PermissionResult result = permission_context.GetPermissionStatus(
        content::PermissionDescriptorUtil::
            CreatePermissionDescriptorForPermissionType(
                blink::PermissionType::GEOLOCATION),
        web_contents()->GetPrimaryMainFrame(), virtual_url, virtual_url);
    EXPECT_EQ(result.status, want_response);
    EXPECT_EQ(result.source, want_source);
  }

  void SetUpUrl(const GURL& url) {
    NavigateAndCommit(url);
    prompt_factory_->DocumentOnLoadCompletedInPrimaryMainFrame();
  }

  void SetIsActorActingOnWebContents(bool is_actor_acting_on_web_contents) {
    client_.is_actor_acting_on_web_contents_ = is_actor_acting_on_web_contents;
  }

 private:
  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    PermissionRequestManager::CreateForWebContents(web_contents());
    PermissionRequestManager* manager =
        PermissionRequestManager::FromWebContents(web_contents());
    manager->set_enabled_app_level_notification_permission_for_testing(true);
    prompt_factory_ = std::make_unique<MockPermissionPromptFactory>(manager);
    ASSERT_EQ(kGeolocationContentSettingsType,
              content_settings::GeolocationContentSettingsType());
  }

  void TearDown() override {
    prompt_factory_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
  PermissionContextBaseTestClient client_;
};

// Simulates clicking Accept. The permission should be granted and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndGrant) {
  TestAskAndDecide_TestContent<ContentSettingsType::NOTIFICATIONS>(
      blink::mojom::PermissionDescriptor::New(
          blink::mojom::PermissionName::NOTIFICATIONS, /*extension=*/nullptr),
      CONTENT_SETTING_ALLOW);
}

// Simulates clicking Block. The permission should be denied and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndBlock) {
  TestAskAndDecide_TestContent<kGeolocationContentSettingsType>(
      blink::mojom::PermissionDescriptor::New(
          blink::mojom::PermissionName::GEOLOCATION, /*extension=*/nullptr),
      CONTENT_SETTING_BLOCK);
}

// Simulates clicking Dismiss (X) in the prompt.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndDismiss) {
  TestAskAndDecide_TestContent<ContentSettingsType::MIDI_SYSEX>(
      blink::mojom::PermissionDescriptor::New(
          blink::mojom::PermissionName::MIDI,
          blink::mojom::PermissionDescriptorExtension::NewMidi(
              blink::mojom::MidiPermissionDescriptor::New(true))),
      CONTENT_SETTING_ASK);
}

// Simulates clicking Dismiss (X) in the prompt with the block on too
// many dismissals feature active. The permission should be blocked after
// several dismissals.
TEST_F(PermissionContextBaseTests, TestDismissUntilBlocked) {
  TestBlockOnSeveralDismissals_TestContent();
}

// Test setting a custom number of dismissals before block via variations.
TEST_F(PermissionContextBaseTests, TestDismissVariations) {
  TestVariationBlockOnSeveralDismissals_TestContent();
}

// Test that contexts that disable the automatic blocker are not blocker.
TEST_F(PermissionContextBaseTests, TestDismissVariationsWithoutEmbargo) {
  TestVariationBlockOnSeveralDismissalsAutomaticEmbargoOff_TestContent();
}

// Simulates non-valid requesting URL.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestNonValidRequestingUrl) {
  TestRequestPermissionInvalidUrl<kGeolocationContentSettingsType>(
      blink::PermissionType::GEOLOCATION);
  TestRequestPermissionInvalidUrl<ContentSettingsType::NOTIFICATIONS>(
      blink::PermissionType::NOTIFICATIONS);
  TestRequestPermissionInvalidUrl<ContentSettingsType::MIDI_SYSEX>(
      blink::PermissionType::MIDI_SYSEX);
}

// Simulates granting and revoking of permissions.
TEST_F(PermissionContextBaseTests, TestGrantAndRevoke) {
  TestGrantAndRevoke_TestContent<kGeolocationContentSettingsType>(
      blink::PermissionType::GEOLOCATION, CONTENT_SETTING_ASK);
  TestGrantAndRevoke_TestContent<ContentSettingsType::MIDI_SYSEX>(
      blink::PermissionType::MIDI_SYSEX, CONTENT_SETTING_ASK);
#if BUILDFLAG(IS_ANDROID)
  TestGrantAndRevoke_TestContent<
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER>(
      blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER, CONTENT_SETTING_ASK);
  // TODO(timvolodine): currently no test for
  // ContentSettingsType::NOTIFICATIONS because notification permissions work
  // differently with infobars as compared to bubbles (crbug.com/453784).
#else
  TestGrantAndRevoke_TestContent<ContentSettingsType::NOTIFICATIONS>(
      blink::PermissionType::NOTIFICATIONS, CONTENT_SETTING_ASK);
#endif
}

// Tests the global kill switch by enabling/disabling the Field Trials.
TEST_F(PermissionContextBaseTests, TestGlobalKillSwitch) {
  TestGlobalPermissionsKillSwitch<kGeolocationContentSettingsType>();
  TestGlobalPermissionsKillSwitch<ContentSettingsType::NOTIFICATIONS>();
  TestGlobalPermissionsKillSwitch<ContentSettingsType::MIDI_SYSEX>();
  TestGlobalPermissionsKillSwitch<ContentSettingsType::PERSISTENT_STORAGE>();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  TestGlobalPermissionsKillSwitch<
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER>();
#endif
  TestGlobalPermissionsKillSwitch<ContentSettingsType::MEDIASTREAM_MIC>();
  TestGlobalPermissionsKillSwitch<ContentSettingsType::MEDIASTREAM_CAMERA>();
}

// Tests that secure origins are examined if switch is on, or ignored if off.
TEST_F(PermissionContextBaseTests,
       TestSecureOriginRestrictedPermissionContextSwitch) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddSecureScheme("chrome-extension");
  struct {
    std::string requesting_url_spec;
    std::string embedding_url_spec;
    bool expect_permission_allowed;
  } kTestCases[] = {
      // Secure-origins that should be allowed.
      {"https://google.com", "https://foo.com",
       /*expect_allowed=*/true},
      {"https://www.bar.com", "https://foo.com",
       /*expect_allowed=*/true},
      {"https://localhost", "http://localhost",
       /*expect_allowed=*/true},

      {"http://localhost", "https://google.com",
       /*expect_allowed=*/true},
      {"https://google.com", "http://localhost",
       /*expect_allowed=*/true},
      {"https://foo.com", "file://some-file",
       /*expect_allowed=*/true},
      {"file://some-file", "https://foo.com",
       /*expect_allowed=*/true},
      {"https://foo.com", "about:blank",
       /*expect_allowed=*/true},
      {"about:blank", "https://foo.com",
       /*expect_allowed=*/true},

      // Extensions are exempt from checking the embedder chain.
      {"chrome-extension://some-extension", "http://not-secure.com",
       /*expect_allowed=*/true},

      // Insecure-origins that should be blocked.
      {"http://foo.com", "file://some-file",
       /*expect_allowed=*/false},
      {"fake://foo.com", "about:blank",
       /*expect_allowed=*/false},
      {"http://localhost", "http://foo.com",
       /*expect_allowed=*/false},
      {"http://localhost", "foo.com",
       /*expect_allowed=*/false},
      {"http://bar.com", "https://foo.com",
       /*expect_permission_allowed=*/false},
      {"https://foo.com", "http://bar.com",
       /*expect_permission_allowed=*/false},
      {"http://localhost", "http://foo.com",
       /*expect_permission_allowed=*/false},
      {"http://foo.com", "http://localhost",
       /*expect_permission_allowed=*/false},
      {"bar.com", "https://foo.com", /*expect_permission_allowed=*/false},
      {"https://foo.com", "bar.com", /*expect_permission_allowed=*/false}};
  for (const auto& test_case : kTestCases) {
    TestSecureOriginRestrictedPermissionContextCheck(
        test_case.requesting_url_spec, test_case.embedding_url_spec,
        test_case.expect_permission_allowed);
  }
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsAllowed) {
  TestParallelRequests(CONTENT_SETTING_ALLOW);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsBlocked) {
  TestParallelRequests(CONTENT_SETTING_BLOCK);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsDismissed) {
  TestParallelRequests(CONTENT_SETTING_ASK);
}

TEST_F(PermissionContextBaseTests, TestVirtualURLDifferentOrigin) {
  TestVirtualURL(GURL("http://www.google.com"), GURL("http://foo.com"),
                 blink::mojom::PermissionStatus::DENIED,
                 content::PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN);
}

TEST_F(PermissionContextBaseTests, TestVirtualURLNotHTTP) {
  TestVirtualURL(GURL("chrome://foo"), GURL("chrome://newtab"),
                 blink::mojom::PermissionStatus::ASK,
                 content::PermissionStatusSource::UNSPECIFIED);
}

TEST_F(PermissionContextBaseTests, TestVirtualURLSameOrigin) {
  TestVirtualURL(GURL("http://www.google.com"),
                 GURL("http://www.google.com/foo"),
                 blink::mojom::PermissionStatus::ASK,
                 content::PermissionStatusSource::UNSPECIFIED);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PermissionContextBaseTests,
       NotificationPermissionDeniedIfNoAppLevelSettings) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kReturnDeniedForNotificationsWhenNoAppLevelSettings);
  base::HistogramTester histograms;
  auto permission_context = CreateTestPermissionContext<
      ContentSettingsType::
          NOTIFICATIONS>(/*enable_app_level_notification_permission=*/
                         false);
  GURL url("https://example.test");
  controller().LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  const PermissionRequestID id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      PermissionRequestID::RequestLocalId());

  auto request_data = std::make_unique<PermissionRequestData>(
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              permissions::PermissionUtil::ContentSettingsTypeToPermissionType(
                  ContentSettingsType::NOTIFICATIONS)),
      id,
      /*user_gesture=*/
      true, url);
  EXPECT_EQ(permission_context.GetPermissionStatus(
                *request_data, web_contents()->GetPrimaryMainFrame()),
            content::PermissionResult(
                content::PermissionStatus::DENIED,
                content::PermissionStatusSource::APP_LEVEL_SETTINGS));

  permission_context.RequestPermission(
      std::move(request_data),
      base::BindOnce(&decltype(permission_context)::TrackPermissionDecision,
                     base::Unretained(&permission_context)));

  ASSERT_EQ(permission_context.permission_statuses().size(), 1u);
  EXPECT_EQ(permission_context.permission_statuses()[0],
            content::PermissionStatus::DENIED);
  EXPECT_TRUE(permission_context.tab_context_updated());
  EXPECT_EQ(PermissionSetting(CONTENT_SETTING_ASK),
            permission_context.GetPermissionSettingFromMap(url, url));
  histograms.ExpectUniqueSample(
      "Permissions.Status.Notifications.EnabledAppLevel", false, 2);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PermissionContextBaseTests, ExpirationAllow) {
  base::Time now = base::Time::Now();
  TestAskAndDecide_TestContent<kGeolocationContentSettingsType>(
      blink::mojom::PermissionDescriptor::New(
          blink::mojom::PermissionName::GEOLOCATION, /*extension=*/nullptr),
      CONTENT_SETTING_ALLOW);

  GURL primary_url("https://www.google.com");
  GURL secondary_url;
  auto* hcsm = PermissionsClient::Get()->GetSettingsMap(browser_context());
  content_settings::SettingInfo info;
  hcsm->GetWebsiteSetting(primary_url, secondary_url,
                          kGeolocationContentSettingsType, &info);

  // The last_visited should lie between today and a week ago.
  EXPECT_GE(info.metadata.last_visited(), now - base::Days(7));
  EXPECT_LE(info.metadata.last_visited(), now);
}

TEST_F(PermissionContextBaseTests, ExpirationBlock) {
  TestAskAndDecide_TestContent<kGeolocationContentSettingsType>(
      blink::mojom::PermissionDescriptor::New(
          blink::mojom::PermissionName::GEOLOCATION, /*extension=*/nullptr),
      CONTENT_SETTING_BLOCK);

  GURL primary_url("https://www.google.com");
  GURL secondary_url;
  auto* hcsm = PermissionsClient::Get()->GetSettingsMap(browser_context());
  content_settings::SettingInfo info;
  hcsm->GetWebsiteSetting(primary_url, secondary_url,
                          kGeolocationContentSettingsType, &info);

  // last_visited is not set for BLOCKed permissions.
  EXPECT_EQ(base::Time(), info.metadata.last_visited());
}

TEST_F(PermissionContextBaseTests, ActorBypass_WhenActive_DeniesPermission) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kGlicActorPermissionsAutoReject}, {});
  SetIsActorActingOnWebContents(true);
  auto permission_context =
      CreateTestPermissionContext<kGeolocationContentSettingsType>();
  GURL url("https://www.google.com");
  SetUpUrl(url);
  base::HistogramTester histograms;
  // Pre-condition: User has granted the permission.
  auto* map = PermissionsClient::Get()->GetSettingsMap(browser_context());
  const content_settings::PermissionSettingsInfo* info =
      content_settings::PermissionSettingsRegistry::GetInstance()->Get(
          kGeolocationContentSettingsType);

  map->SetPermissionSettingDefaultScope(
      url, url, kGeolocationContentSettingsType,
      info->delegate().ToPermissionSetting(CONTENT_SETTING_ALLOW));

  content::PermissionResult result = permission_context.GetPermissionStatus(
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              permissions::PermissionUtil::ContentSettingsTypeToPermissionType(
                  permission_context.content_settings_type())),
      web_contents()->GetPrimaryMainFrame(), url, url);

  EXPECT_EQ(PermissionStatus::DENIED, result.status);
  EXPECT_EQ(content::PermissionStatusSource::ACTOR_OVERRIDE, result.source);
  histograms.ExpectBucketCount(
      "Permissions.Experimental.Usage." +
          PermissionUtil::GetPermissionString(
              permission_context.content_settings_type()) +
          ".IsBlockedDueToActuation",
      true, 1);
}

TEST_F(PermissionContextBaseTests,
       ActorBypass_WhenInactive_RespectsPermission) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kGlicActorPermissionsAutoReject}, {});
  // Actor is not currently active on the web contents.
  SetIsActorActingOnWebContents(false);
  auto permission_context =
      CreateTestPermissionContext<kGeolocationContentSettingsType>();
  GURL url("https://www.google.com");
  SetUpUrl(url);
  base::HistogramTester histograms;
  // Pre-condition: User has granted the permission.
  auto* map = PermissionsClient::Get()->GetSettingsMap(browser_context());
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::GEOLOCATION,
                                     CONTENT_SETTING_ALLOW);

  content::PermissionResult result = permission_context.GetPermissionStatus(
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              permissions::PermissionUtil::ContentSettingsTypeToPermissionType(
                  permission_context.content_settings_type())),
      web_contents()->GetPrimaryMainFrame(), url, url);

  // The original user setting of GRANTED is respected.
  EXPECT_EQ(PermissionStatus::GRANTED, result.status);
  histograms.ExpectBucketCount(
      "Permissions.Experimental.Usage." +
          PermissionUtil::GetPermissionString(
              permission_context.content_settings_type()) +
          ".IsBlockedDueToActuation",
      false, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace permissions
