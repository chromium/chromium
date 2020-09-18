// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/test/test_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {

const char kWebAppTitle[] = "Foo Title";

const char kSystemAppExtensionInstalErrorHistogramName[] =
    "Webapp.InstallResultExtensionError.System.Profiles.Other";
const char kSystemAppExtensionDisableReasonHistogramName[] =
    "Webapp.InstallResultExtensionDisabledReason.System.Profiles.Other";

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL WebAppUrl() {
  return GURL("https://foo.example");
}
GURL AlternateWebAppUrl() {
  return GURL("https://bar.example");
}

}  // namespace

// Do not add tests to this class. Instead, add tests to
// |InstallFinalizerUnitTest| so that both |InstallFinalizer| implementations
// are tested.
class BookmarkAppInstallFinalizerTest : public ChromeRenderViewHostTestHarness {
 public:
  // Subclass that runs a closure when an extension is unpacked successfully.
  // Useful for tests that want to trigger their own success/failure events.
  class FakeCrxInstaller : public CrxInstaller {
   public:
    explicit FakeCrxInstaller(Profile* profile)
        : CrxInstaller(
              ExtensionSystem::Get(profile)->extension_service()->AsWeakPtr(),
              nullptr,
              nullptr) {}

    void OnUnpackSuccessOnSharedFileThread(
        base::FilePath temp_dir,
        base::FilePath extension_dir,
        std::unique_ptr<base::DictionaryValue> original_manifest,
        scoped_refptr<const Extension> extension,
        SkBitmap install_icon,
        declarative_net_request::RulesetInstallPrefs ruleset_install_prefs)
        override {
      run_loop_.Quit();
    }

    void WaitForInstallToTrigger() { run_loop_.Run(); }

    void SimulateInstallFailed() {
      CrxInstallError error(CrxInstallErrorType::DECLINED,
                            CrxInstallErrorDetail::INSTALL_NOT_ENABLED,
                            base::ASCIIToUTF16(""));
      NotifyCrxInstallComplete(error);
    }

   private:
    ~FakeCrxInstaller() override = default;

    base::RunLoop run_loop_;

    DISALLOW_COPY_AND_ASSIGN(FakeCrxInstaller);
  };

  class TestCrxInstallerToDisableExtension : public CrxInstaller {
   public:
    explicit TestCrxInstallerToDisableExtension(
        Profile* profile,
        int disable_reason = disable_reason::DisableReason::DISABLE_NONE)
        : CrxInstaller(
              ExtensionSystem::Get(profile)->extension_service()->AsWeakPtr(),
              nullptr,
              nullptr),
          disable_reason_(disable_reason),
          profile_(profile) {}

    void set_installer_callback(InstallerResultCallback callback) override {
      real_callback_ = std::move(callback);
    }

    void InstallWebApp(const WebApplicationInfo& web_app) override {
      CrxInstaller::set_installer_callback(base::BindOnce(
          &TestCrxInstallerToDisableExtension::OnCrxInstalled, this));
      CrxInstaller::InstallWebApp(web_app);
    }

   private:
    ~TestCrxInstallerToDisableExtension() override = default;

    void OnCrxInstalled(const base::Optional<CrxInstallError>& result) {
      ExtensionSystem::Get(profile_)->extension_service()->DisableExtension(
          extension()->id(), disable_reason_);
      std::move(real_callback_).Run(result);
    }

    int disable_reason_;

    Profile* profile_;

    InstallerResultCallback real_callback_;

    DISALLOW_COPY_AND_ASSIGN(TestCrxInstallerToDisableExtension);
  };

  BookmarkAppInstallFinalizerTest() = default;
  ~BookmarkAppInstallFinalizerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // CrxInstaller in BookmarkAppInstallFinalizer needs an ExtensionService, so
    // create one for the profile.
    TestExtensionSystem* test_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false /* autoupdate_enabled */);

    registrar_ = std::make_unique<BookmarkAppRegistrar>(profile());
    registry_controller_ =
        std::make_unique<web_app::TestAppRegistryController>(profile());
    ui_manager_ = std::make_unique<web_app::TestWebAppUiManager>();

    finalizer_ = std::make_unique<BookmarkAppInstallFinalizer>(profile());
    finalizer_->SetSubsystems(registrar_.get(), ui_manager_.get(),
                              registry_controller_.get());
  }

  web_app::AppId InstallExternalApp(const GURL& start_url) {
    auto info = std::make_unique<WebApplicationInfo>();
    info->start_url = start_url;
    info->title = base::ASCIIToUTF16(kWebAppTitle);

    web_app::InstallFinalizer::FinalizeOptions options;
    options.install_source = WebappInstallSource::EXTERNAL_POLICY;

    web_app::AppId app_id;
    base::RunLoop run_loop;
    finalizer().FinalizeInstall(
        *info, options,
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          ASSERT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
          app_id = installed_app_id;
          run_loop.Quit();
        }));
    run_loop.Run();

    web_app::ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
        .Insert(start_url, app_id,
                web_app::ExternalInstallSource::kExternalPolicy);

    return app_id;
  }

  void SimulateExternalAppUninstalledByUser(const web_app::AppId& app_id) {
    ExtensionRegistry::Get(profile())->RemoveEnabled(app_id);
    auto* extension_prefs = ExtensionPrefs::Get(profile());
    extension_prefs->OnExtensionUninstalled(app_id, Manifest::EXTERNAL_POLICY,
                                            false /* external_uninstall */);
    DCHECK(extension_prefs->IsExternalExtensionUninstalled(app_id));
  }

  const ExtensionSet& enabled_extensions() {
    return ExtensionRegistry::Get(profile())->enabled_extensions();
  }

  BookmarkAppInstallFinalizer& finalizer() { return *finalizer_; }

 private:
  std::unique_ptr<BookmarkAppRegistrar> registrar_;
  std::unique_ptr<web_app::TestAppRegistryController> registry_controller_;
  std::unique_ptr<web_app::TestWebAppUiManager> ui_manager_;
  std::unique_ptr<BookmarkAppInstallFinalizer> finalizer_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallFinalizerTest);
};

TEST_F(BookmarkAppInstallFinalizerTest, BasicInstallButExtensionIsDisabled) {
  base::HistogramTester histograms;

  auto test_crx_installer_to_disable_extension = base::MakeRefCounted<
      BookmarkAppInstallFinalizerTest::TestCrxInstallerToDisableExtension>(
      profile(), disable_reason::DisableReason::DISABLE_BLOCKED_BY_POLICY);

  finalizer().SetCrxInstallerFactoryForTesting(
      base::BindLambdaForTesting([&](Profile* profile) {
        scoped_refptr<CrxInstaller> crx_installer =
            test_crx_installer_to_disable_extension;
        return crx_installer;
      }));

  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::INTERNAL_DEFAULT;

  base::RunLoop run_loop;
  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kWebAppDisabled, code);

        // Non System Web App disable reason is not recorded.
        histograms.ExpectTotalCount(
            kSystemAppExtensionDisableReasonHistogramName, 0);

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, BasicInstallFails) {
  base::HistogramTester histograms;

  auto fake_crx_installer =
      base::MakeRefCounted<BookmarkAppInstallFinalizerTest::FakeCrxInstaller>(
          profile());

  finalizer().SetCrxInstallerFactoryForTesting(
      base::BindLambdaForTesting([&](Profile* profile) {
        scoped_refptr<CrxInstaller> crx_installer = fake_crx_installer;
        return crx_installer;
      }));

  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  base::RunLoop run_loop;
  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::INTERNAL_DEFAULT;
  bool callback_called = false;

  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kBookmarkExtensionInstallError,
                  code);
        EXPECT_TRUE(installed_app_id.empty());
        // Non System Web App install failures are not recorded.
        histograms.ExpectTotalCount(kSystemAppExtensionInstalErrorHistogramName,
                                    0);
        callback_called = true;
        run_loop.Quit();
      }));

  fake_crx_installer->WaitForInstallToTrigger();
  fake_crx_installer->SimulateInstallFailed();
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallFinalizerTest, ConcurrentInstallSucceeds) {
  base::RunLoop run_loop;

  const GURL url1("https://foo1.example");
  const GURL url2("https://foo2.example");

  bool callback1_called = false;
  bool callback2_called = false;
  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::INTERNAL_DEFAULT;

  // Start install finalization for the 1st app
  {
    WebApplicationInfo web_application_info;
    web_application_info.start_url = url1;

    finalizer().FinalizeInstall(
        web_application_info, options,
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(installed_app_id, web_app::GenerateAppIdFromURL(url1));
          callback1_called = true;
          if (callback2_called)
            run_loop.Quit();
        }));
  }

  // Start install finalization for the 2nd app
  {
    WebApplicationInfo web_application_info;
    web_application_info.start_url = url2;

    finalizer().FinalizeInstall(
        web_application_info, options,
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(installed_app_id, web_app::GenerateAppIdFromURL(url2));
          callback2_called = true;
          if (callback1_called)
            run_loop.Quit();
        }));
  }

  run_loop.Run();

  EXPECT_TRUE(callback1_called);
  EXPECT_TRUE(callback2_called);
}

TEST_F(BookmarkAppInstallFinalizerTest, DefaultInstalledSucceeds) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::EXTERNAL_DEFAULT;

  base::RunLoop run_loop;
  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);

        auto* extension =
            ExtensionRegistry::Get(profile())->GetInstalledExtension(
                installed_app_id);
        EXPECT_TRUE(Manifest::IsExternalLocation(extension->location()));
        EXPECT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD, extension->location());
        EXPECT_TRUE(extension->was_installed_by_default());

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, PolicyInstalledSucceeds) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::EXTERNAL_POLICY;

  base::RunLoop run_loop;
  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);

        auto* extension =
            ExtensionRegistry::Get(profile())->GetInstalledExtension(
                installed_app_id);
        EXPECT_TRUE(Manifest::IsPolicyLocation(extension->location()));
        EXPECT_TRUE(BookmarkAppIsLocallyInstalled(profile(), extension));

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, SystemInstalledSucceeds) {
  base::HistogramTester histograms;
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::SYSTEM_DEFAULT;

  base::RunLoop run_loop;
  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);

        auto* extension =
            ExtensionRegistry::Get(profile())->GetInstalledExtension(
                installed_app_id);
        EXPECT_TRUE(Manifest::IsExternalLocation(extension->location()));
        EXPECT_EQ(Manifest::EXTERNAL_COMPONENT, extension->location());
        EXPECT_TRUE(extension->was_installed_by_default());

        // Successful install does not record histogram.
        histograms.ExpectTotalCount(kSystemAppExtensionInstalErrorHistogramName,
                                    0);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, SystemInstalledButExtensionIsDisabled) {
  base::HistogramTester histograms;

  auto test_crx_installer_to_disable_extension = base::MakeRefCounted<
      BookmarkAppInstallFinalizerTest::TestCrxInstallerToDisableExtension>(
      profile(), disable_reason::DisableReason::DISABLE_BLOCKED_BY_POLICY);

  finalizer().SetCrxInstallerFactoryForTesting(
      base::BindLambdaForTesting([&](Profile* profile) {
        scoped_refptr<CrxInstaller> crx_installer =
            test_crx_installer_to_disable_extension;
        return crx_installer;
      }));

  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::SYSTEM_DEFAULT;

  base::RunLoop run_loop;
  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kWebAppDisabled, code);

        histograms.ExpectBucketCount(
            kSystemAppExtensionDisableReasonHistogramName,
            disable_reason::DisableReason::DISABLE_BLOCKED_BY_POLICY, 1);

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, SystemInstalledFails) {
  base::HistogramTester histograms;

  auto fake_crx_installer =
      base::MakeRefCounted<BookmarkAppInstallFinalizerTest::FakeCrxInstaller>(
          profile());

  finalizer().SetCrxInstallerFactoryForTesting(
      base::BindLambdaForTesting([&](Profile* profile) {
        scoped_refptr<CrxInstaller> crx_installer = fake_crx_installer;
        return crx_installer;
      }));

  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::SYSTEM_DEFAULT;

  base::RunLoop run_loop;
  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kBookmarkExtensionInstallError,
                  code);

        histograms.ExpectBucketCount(
            kSystemAppExtensionInstalErrorHistogramName,
            CrxInstallErrorDetail::INSTALL_NOT_ENABLED, 1);

        run_loop.Quit();
      }));

  fake_crx_installer->WaitForInstallToTrigger();
  fake_crx_installer->SimulateInstallFailed();

  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, NoNetworkInstallForArc) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::ARC;

  base::RunLoop run_loop;
  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);

        auto* extension =
            ExtensionRegistry::Get(profile())->GetInstalledExtension(
                installed_app_id);
        EXPECT_TRUE(Manifest::IsExternalLocation(extension->location()));
        EXPECT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD, extension->location());

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, UninstallExternalWebApp_Successful) {
  InstallExternalApp(WebAppUrl());
  ASSERT_EQ(1u, enabled_extensions().size());

  base::RunLoop run_loop;
  finalizer().UninstallExternalWebAppByUrl(
      WebAppUrl(), web_app::ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_TRUE(uninstalled);
        EXPECT_EQ(0u, enabled_extensions().size());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, UninstallExternalWebApp_Multiple) {
  auto foo_app_id = InstallExternalApp(WebAppUrl());
  auto bar_app_id = InstallExternalApp(AlternateWebAppUrl());
  ASSERT_EQ(2u, enabled_extensions().size());

  // Uninstall one app.
  {
    base::RunLoop run_loop;
    finalizer().UninstallExternalWebAppByUrl(
        WebAppUrl(), web_app::ExternalInstallSource::kExternalPolicy,
        base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  EXPECT_EQ(1u, enabled_extensions().size());
  EXPECT_TRUE(enabled_extensions().Contains(bar_app_id));

  // Uninstall the second app.
  {
    base::RunLoop run_loop;
    finalizer().UninstallExternalWebAppByUrl(
        AlternateWebAppUrl(), web_app::ExternalInstallSource::kExternalPolicy,
        base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  EXPECT_EQ(0u, enabled_extensions().size());
}

TEST_F(BookmarkAppInstallFinalizerTest,
       UninstallExternalWebApp_UninstalledExternalApp) {
  auto app_id = InstallExternalApp(WebAppUrl());
  SimulateExternalAppUninstalledByUser(app_id);

  base::RunLoop run_loop;
  finalizer().UninstallExternalWebAppByUrl(
      WebAppUrl(), web_app::ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_FALSE(uninstalled);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest,
       UninstallExternalWebApp_FailsNeverInstalled) {
  base::RunLoop run_loop;
  finalizer().UninstallExternalWebAppByUrl(
      WebAppUrl(), web_app::ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_FALSE(uninstalled);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest,
       UninstallExternalWebApp_FailsAlreadyUninstalled) {
  InstallExternalApp(WebAppUrl());

  // Uninstall the app.
  {
    base::RunLoop run_loop;
    finalizer().UninstallExternalWebAppByUrl(
        WebAppUrl(), web_app::ExternalInstallSource::kExternalPolicy,
        base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Try to uninstall it again.
  {
    base::RunLoop run_loop;
    finalizer().UninstallExternalWebAppByUrl(
        WebAppUrl(), web_app::ExternalInstallSource::kExternalPolicy,
        base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_FALSE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(BookmarkAppInstallFinalizerTest, NotLocallyInstalled) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::INTERNAL_DEFAULT;
  options.locally_installed = false;

  base::RunLoop run_loop;
  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);

        auto* extension =
            ExtensionRegistry::Get(profile())->GetInstalledExtension(
                installed_app_id);
        EXPECT_FALSE(BookmarkAppIsLocallyInstalled(profile(), extension));

        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace extensions
