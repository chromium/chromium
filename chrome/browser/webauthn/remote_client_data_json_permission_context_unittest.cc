// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/remote_client_data_json_permission_context.h"

#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kAuthorizedOrigin[] = "https://acme.example";
constexpr char kUnauthorizedOrigin[] = "https://intruder.example";

class TestPermissionObserver : public permissions::Observer {
 public:
  void OnPermissionChanged(const ContentSettingsPattern& primary_pattern,
                           const ContentSettingsPattern& secondary_pattern,
                           ContentSettingsTypeSet content_type_set) override {
    EXPECT_EQ(primary_pattern, ContentSettingsPattern::Wildcard());
    EXPECT_EQ(secondary_pattern, ContentSettingsPattern::Wildcard());
    EXPECT_TRUE(content_type_set.Contains(
        ContentSettingsType::PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON));
    ++notification_count_;
  }

  int notification_count() const { return notification_count_; }

 private:
  int notification_count_ = 0;
};

// Exposes the protected status accessor for testing.
class TestRemoteClientDataJsonPermissionContext
    : public RemoteClientDataJsonPermissionContext {
 public:
  using RemoteClientDataJsonPermissionContext::GetContentSettingStatusInternal;
  using RemoteClientDataJsonPermissionContext::
      RemoteClientDataJsonPermissionContext;
};

class RemoteClientDataJsonPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  ContentSetting StatusFor(const char* origin) {
    TestRemoteClientDataJsonPermissionContext context(GetBrowserContext());
    return context.GetContentSettingStatusInternal(
        /*render_frame_host=*/nullptr, GURL(origin), GURL(origin));
  }

  // Sets the allowed origins pref in the managed pref store, as the
  // WebAuthenticationRemoteDesktopAllowedOrigins policy would.
  void SetAllowedOrigins(std::initializer_list<const char*> origins) {
    base::ListValue list;
    for (const char* origin : origins) {
      list.Append(origin);
    }
    profile()->GetTestingPrefService()->SetManagedPref(
        webauthn::pref_names::kRemoteDesktopAllowedOrigins, std::move(list));
  }

  // Appends a cross-origin iframe that is delegated the permissions policy.
  content::RenderFrameHost* AddChildFrame(const GURL& url) {
    network::ParsedPermissionsPolicy frame_policy = {
        network::ParsedPermissionsPolicyDeclaration(
            network::mojom::PermissionsPolicyFeature::
                kPublicKeyCredentialsRemoteClientDataJson,
            {*network::OriginWithPossibleWildcards::FromOrigin(
                url::Origin::Create(url))},
            /*self_if_matches=*/std::nullopt,
            /*matches_all_origins=*/false,
            /*matches_opaque_src=*/false)};
    content::RenderFrameHost* child =
        content::RenderFrameHostTester::For(main_rfh())
            ->AppendChildWithPolicy("child", frame_policy);
    return content::NavigationSimulator::NavigateAndCommitFromDocument(url,
                                                                       child);
  }

  blink::mojom::PermissionStatus PermissionStatusFor(
      content::RenderFrameHost* render_frame_host) {
    content::PermissionControllerDelegate* delegate =
        PermissionManagerFactory::GetForProfile(profile());
    return delegate
        ->GetPermissionResultForCurrentDocument(
            content::PermissionDescriptorUtil::
                CreatePermissionDescriptorForPermissionType(
                    blink::PermissionType::
                        PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON),
            render_frame_host, /*should_include_device_status=*/false)
        .status;
  }

  // Requests the permission for `render_frame_host` through the permission
  // manager, as a page would, and returns the resulting status.
  blink::mojom::PermissionStatus RequestPermissionFor(
      content::RenderFrameHost* render_frame_host) {
    content::PermissionControllerDelegate* delegate =
        PermissionManagerFactory::GetForProfile(profile());
    base::test::TestFuture<const std::vector<content::PermissionResult>&>
        future;
    delegate->RequestPermissionsFromCurrentDocument(
        render_frame_host,
        content::PermissionRequestDescription(
            content::PermissionDescriptorUtil::
                CreatePermissionDescriptorForPermissionType(
                    blink::PermissionType::
                        PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON),
            /*user_gesture=*/true),
        future.GetCallback());
    const std::vector<content::PermissionResult>& results = future.Get();
    if (results.size() != 1u) {
      ADD_FAILURE() << "Expected one result, got " << results.size();
      return blink::mojom::PermissionStatus::ASK;
    }
    return results[0].status;
  }

  PrefService* prefs() {
    return Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  }

  // The enterprise-policy authorization path may require an affiliated profile
  // (see RemoteDesktopClientOverrideAllowedByPolicy()), so tests exercising
  // that path mark the profile affiliated to stay independent of how
  // affiliation is enforced.
  void SetProfileAffiliated() {
    const base::flat_set<std::string> affiliation_ids({"test-affiliation-id"});
    g_browser_process->browser_policy_connector()
        ->SetDeviceAffiliatedIdsForTesting(affiliation_ids);
    Profile::FromBrowserContext(GetBrowserContext())
        ->GetProfilePolicyConnector()
        ->SetUserAffiliationIdsForTesting(affiliation_ids);
  }
};

// With no enterprise policy, every origin resolves to BLOCK.
TEST_F(RemoteClientDataJsonPermissionContextTest, BlockedByDefault) {
  EXPECT_EQ(StatusFor(kAuthorizedOrigin), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(StatusFor(kUnauthorizedOrigin), CONTENT_SETTING_BLOCK);
}

#if !BUILDFLAG(IS_CHROMEOS)
// The WebAuthenticationRemoteDesktopAllowedOrigins enterprise policy authorizes
// the listed origin (ALLOW). The profile is marked affiliated (see
// SetProfileAffiliated()); the affiliation interactions are covered by
// ChromeWebAuthenticationDelegateBase's own unit tests and not duplicated here.
TEST_F(RemoteClientDataJsonPermissionContextTest, AllowedByEnterprisePolicy) {
  SetProfileAffiliated();
  SetAllowedOrigins({kAuthorizedOrigin});

  EXPECT_EQ(StatusFor(kAuthorizedOrigin), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(StatusFor(kUnauthorizedOrigin), CONTENT_SETTING_BLOCK);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)
// A cross-origin iframe is granted the permission when its own origin is
// authorized, matching WebAuthn's caller-origin authorization.
TEST_F(RemoteClientDataJsonPermissionContextTest, CrossOriginIframe) {
  constexpr char kTopLevelOrigin[] = "https://portal.example";
  SetProfileAffiliated();
  NavigateAndCommit(GURL(kTopLevelOrigin));
  content::RenderFrameHost* child = AddChildFrame(GURL(kAuthorizedOrigin));

  SetAllowedOrigins({kAuthorizedOrigin});
  EXPECT_EQ(PermissionStatusFor(child),
            blink::mojom::PermissionStatus::GRANTED);

  SetAllowedOrigins({kTopLevelOrigin});
  EXPECT_EQ(PermissionStatusFor(main_rfh()),
            blink::mojom::PermissionStatus::GRANTED);
  EXPECT_EQ(PermissionStatusFor(child), blink::mojom::PermissionStatus::DENIED);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(RemoteClientDataJsonPermissionContextTest,
       NotifiesWhenEnterprisePolicyChanges) {
  SetProfileAffiliated();
  TestRemoteClientDataJsonPermissionContext context(GetBrowserContext());
  TestPermissionObserver observer;
  context.AddObserver(&observer);
  const auto status_for = [&context](const char* origin) {
    return context.GetContentSettingStatusInternal(
        /*render_frame_host=*/nullptr, GURL(origin), GURL(origin));
  };

  EXPECT_EQ(status_for(kAuthorizedOrigin), CONTENT_SETTING_BLOCK);

  SetAllowedOrigins({kAuthorizedOrigin});
  EXPECT_EQ(observer.notification_count(), 1);
  EXPECT_EQ(status_for(kAuthorizedOrigin), CONTENT_SETTING_ALLOW);

  profile()->GetTestingPrefService()->RemoveManagedPref(
      webauthn::pref_names::kRemoteDesktopAllowedOrigins);
  EXPECT_EQ(observer.notification_count(), 2);
  EXPECT_EQ(status_for(kAuthorizedOrigin), CONTENT_SETTING_BLOCK);

  prefs()->SetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed,
                      true);
  EXPECT_EQ(observer.notification_count(), 3);

  context.RemoveObserver(&observer);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// An origin without policy authorization is blocked, not prompted.
TEST_F(RemoteClientDataJsonPermissionContextTest, NeverPrompts) {
  EXPECT_NE(StatusFor(kUnauthorizedOrigin), CONTENT_SETTING_ASK);
}

// A request is resolved from the policy-derived status without reaching
// DecidePermission(), which is NOTREACHED().
TEST_F(RemoteClientDataJsonPermissionContextTest, RequestWithoutPolicyDenied) {
  NavigateAndCommit(GURL(kAuthorizedOrigin));
  EXPECT_EQ(RequestPermissionFor(main_rfh()),
            blink::mojom::PermissionStatus::DENIED);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(RemoteClientDataJsonPermissionContextTest,
       RequestResolvedByEnterprisePolicy) {
  SetProfileAffiliated();
  SetAllowedOrigins({kAuthorizedOrigin});

  NavigateAndCommit(GURL(kAuthorizedOrigin));
  EXPECT_EQ(RequestPermissionFor(main_rfh()),
            blink::mojom::PermissionStatus::GRANTED);

  NavigateAndCommit(GURL(kUnauthorizedOrigin));
  EXPECT_EQ(RequestPermissionFor(main_rfh()),
            blink::mojom::PermissionStatus::DENIED);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// A request from a privileged surface, such as the New Tab Page or the omnibox
// popup, can treat a BLOCK status as ignorable and reach DecidePermission().
// It must still be denied.
TEST_F(RemoteClientDataJsonPermissionContextTest,
       RequestFromPrivilegedSurfaceDenied) {
  for (const GURL& surface : {chrome::ChromeUINewTabPageURLAsGURL(),
                              GURL(chrome::kChromeUIOmniboxPopupURL)}) {
    SCOPED_TRACE(surface.spec());
    NavigateAndCommit(GURL(kUnauthorizedOrigin));
    content::OverrideLastCommittedOrigin(main_rfh(),
                                         url::Origin::Create(surface));
    EXPECT_EQ(RequestPermissionFor(main_rfh()),
              blink::mojom::PermissionStatus::DENIED);
  }
}

}  // namespace
