// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "device/fido/public/features.h"
#include "device/fido/virtual_fido_device_factory.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#endif

namespace {

static constexpr char kGetAssertionCredID1234ExampleCom[] = R"((() => {
  let cred_id = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
  return navigator.credentials.get({
    publicKey: {
      challenge: cred_id,
      rpId: "www.example.com",
      timeout: 10000,
      userVerification: 'discouraged',
      allowCredentials: [{ type: 'public-key', id: cred_id }],
      extensions: {
        remoteDesktopClientOverride: {
          origin: "https://www.example.com",
          sameOriginWithAncestors: true
        }
      }
    }
  }).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

static constexpr char kMakeCredentialCrossDomainIWA[] = R"((() => {
      return navigator.credentials.create({ publicKey: {
        rp: { id: "www.example.com", name: "" },
        user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
        pubKeyCredParams: [{type: "public-key", alg: -7}],
        challenge: new Uint8Array([0]),
        timeout: 10000,
        userVerification: 'discouraged',
        extensions: {
          remoteDesktopClientOverride: {
                  origin: "https://www.example.com",
                  sameOriginWithAncestors: true
                }
        },
      }}).then(c => 'webauthn: OK',
              e => 'error ' + e);
    })())";

static constexpr uint8_t kCredentialID[] = {1, 2,  3,  4,  5,  6,  7,  8,
                                            9, 10, 11, 12, 13, 14, 15, 16};

std::string PrintParam(testing::TestParamInfo<bool> is_affiliated) {
  return base::StringPrintf("%sAffiliated", is_affiliated.param ? "" : "Not");
}
// This file tests WebAuthn features that depend on user affiliation (user
// is expected to be affiliated to use remoteDesktopClientOverride on CrOS)
class WebAuthnIWARemoteDesktopOverrideBrowserTest :
#if BUILDFLAG(IS_CHROMEOS)
    public policy::DevicePolicyCrosBrowserTest,
#else
    public InProcessBrowserTest,
#endif
    public ::testing::WithParamInterface<bool> {
 public:
  WebAuthnIWARemoteDesktopOverrideBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS)
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    crypto_home_mixin_.ApplyAuthConfig(
        affiliation_mixin_.account_id(),
        ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));

    affiliation_mixin_.set_affiliated(GetParam());
#endif
    scoped_feature_list_.InitWithFeatures(
        {device::kWebAuthnIWARemoteDesktopAllowedOriginsPolicy,
         features::kIsolatedWebApps},
        {});
  }

  WebAuthnIWARemoteDesktopOverrideBrowserTest(
      const WebAuthnIWARemoteDesktopOverrideBrowserTest&) = delete;
  WebAuthnIWARemoteDesktopOverrideBrowserTest& operator=(
      const WebAuthnIWARemoteDesktopOverrideBrowserTest&) = delete;

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }
#endif

  Profile* profile() {
#if BUILDFLAG(IS_CHROMEOS)
    return ::ash::ProfileHelper::Get()->GetProfileByAccountId(
        affiliation_mixin_.account_id());
#else
    return browser()->profile();
#endif
  }

  void VerifyAffiliationExpectations() {
#if BUILDFLAG(IS_CHROMEOS)
    const user_manager::User* user = user_manager::UserManager::Get()->FindUser(
        affiliation_mixin_.account_id());
    EXPECT_EQ(GetParam(), user->IsAffiliated());
    EXPECT_TRUE(user->is_logged_in());
#else
    ASSERT_TRUE(profile());
#endif
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web_app::OsIntegrationTestOverrideBlockingRegistration
      os_integration_override_;
#if BUILDFLAG(IS_CHROMEOS)
  policy::DevicePolicyCrosTestHelper test_helper_;
  policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
#endif

  content::RenderFrameHost* OpenApp(const webapps::AppId& app_id,
                                    Profile* profile) {
    auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
    auto url = std::nullopt;

    base::test::TestFuture<content::WebContents*> future;
    provider->scheduler().LaunchApp(
        app_id, url,
        base::BindOnce([](base::WeakPtr<Browser>,
                          base::WeakPtr<content::WebContents> web_contents,
                          apps::LaunchContainer) {
          return web_contents.get();
        }).Then(future.GetCallback()));

    auto* web_contents = future.Get();
    content::WaitForLoadStop(web_contents);
    return web_contents->GetPrimaryMainFrame();
  }
};

IN_PROC_BROWSER_TEST_P(WebAuthnIWARemoteDesktopOverrideBrowserTest,
                       PRE_IWAsPolicyAndPrefsSetDifferentRp_id) {
#if BUILDFLAG(IS_CHROMEOS)
  policy::AffiliationTestHelper::PreLoginUser(affiliation_mixin_.account_id());
#endif
}

IN_PROC_BROWSER_TEST_P(WebAuthnIWARemoteDesktopOverrideBrowserTest,
                       IWAsPolicyAndPrefsSetDifferentRp_id) {
  // Test that WebAuthn works inside of IWAs (call to navigator.credentials.get
  // allowed and processed). Positive case with policy and prefs set, rp_id
  // not equals to IWAs caller origin, set in extension, and affiliated/non
  // affiliated user
#if BUILDFLAG(IS_CHROMEOS)
  policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
#endif

  VerifyAffiliationExpectations();

  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder()
              // Allow IWA to create and get creds, without permission to create
              // a cred we will face '"error NotAllowedError:
              // The 'publickey-credentials-create' feature is not enabled in
              // this document. Permissions Policy may be used to delegate Web
              // Authentication capabilities to cross-origin child frames."'
              .AddPermissionsPolicy(network::mojom::PermissionsPolicyFeature::
                                        kPublicKeyCredentialsCreate,
                                    true, {})
              .AddPermissionsPolicy(network::mojom::PermissionsPolicyFeature::
                                        kPublicKeyCredentialsGet,
                                    true, {}))
          .BuildBundle();

  auto url_info = app->Install(profile());
  // Put IWA origin to prefs to emulate values set by
  // WebAuthenticationRemoteDesktopAllowedOrigins policy
  PrefService* prefs = Profile::FromBrowserContext(profile())->GetPrefs();
  base::ListValue list =
      base::ListValue().Append("isolated-app://" + url_info->origin().host());
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 std::move(list));

  auto virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();

  // Create and register creds with rp_id equals to caller origin set in
  // remoteDesktopClientOverride extension below
  EXPECT_TRUE(virtual_device_factory->mutable_state()->InjectRegistration(
      kCredentialID, "www.example.com"));

  content::ScopedAuthenticatorEnvironmentForTesting auth_env(
      std::move(virtual_device_factory));

  content::RenderFrameHost* app_frame = OpenApp(url_info->app_id(), profile());

  if (GetParam()) {
    EXPECT_EQ("webauthn: OK",
              content::EvalJs(app_frame, kMakeCredentialCrossDomainIWA));

    EXPECT_EQ("webauthn: OK",
              content::EvalJs(app_frame, kGetAssertionCredID1234ExampleCom));
  } else {
#if BUILDFLAG(IS_CHROMEOS)
    // Not allowed to use remoteDesktopClientOverride due to missing
    // affiliation, CrOS only
    EXPECT_EQ(
        "error NotAllowedError: This origin is not permitted to use the "
        "'remoteDesktopClientOverride' extension.",
        content::EvalJs(app_frame, kMakeCredentialCrossDomainIWA));

    // Not allowed to use remoteDesktopClientOverride due to missing
    // affiliation. CrOS only
    EXPECT_EQ(
        "error NotAllowedError: This origin is not permitted to use the "
        "'remoteDesktopClientOverride' extension.",
        content::EvalJs(app_frame, kGetAssertionCredID1234ExampleCom));
#else
    EXPECT_EQ("webauthn: OK",
              content::EvalJs(app_frame, kMakeCredentialCrossDomainIWA));

    EXPECT_EQ("webauthn: OK",
              content::EvalJs(app_frame, kGetAssertionCredID1234ExampleCom));
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
}

INSTANTIATE_TEST_SUITE_P(WebAuthnAffiliationCheck,
                         WebAuthnIWARemoteDesktopOverrideBrowserTest,
                         ::testing::Bool(),
                         PrintParam);

}  // namespace
