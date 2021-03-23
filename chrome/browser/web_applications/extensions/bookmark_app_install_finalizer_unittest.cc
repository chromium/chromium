// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/test/test_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_os_integration_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
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
// TODO(crbug.com/1068081): Migrate remaining tests to
// install_finalizer_unittest.
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
                            CrxInstallErrorDetail::INSTALL_NOT_ENABLED, u"");
      NotifyCrxInstallComplete(error);
    }
    FakeCrxInstaller(const FakeCrxInstaller&) = delete;
    FakeCrxInstaller& operator=(const FakeCrxInstaller&) = delete;

   private:
    ~FakeCrxInstaller() override = default;

    base::RunLoop run_loop_;
  };

  BookmarkAppInstallFinalizerTest() = default;
  BookmarkAppInstallFinalizerTest(const BookmarkAppInstallFinalizerTest&) =
      delete;
  BookmarkAppInstallFinalizerTest& operator=(
      const BookmarkAppInstallFinalizerTest&) = delete;
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
    os_integration_manager_ =
        std::make_unique<web_app::TestOsIntegrationManager>(
            profile(), /*shortcut_manager=*/nullptr,
            /*file_handler_manager=*/nullptr,
            /*protocol_handler_manager=*/nullptr,
            /*url_handler_manager=*/nullptr);

    finalizer_ = std::make_unique<BookmarkAppInstallFinalizer>(profile());
    finalizer_->SetSubsystems(registrar_.get(), ui_manager_.get(),
                              registry_controller_.get(),
                              os_integration_manager_.get());
    registrar_->SetSubsystems(os_integration_manager_.get());
  }

  web_app::AppId InstallExternalApp(const GURL& start_url) {
    auto info = std::make_unique<WebApplicationInfo>();
    info->start_url = start_url;
    info->title = base::ASCIIToUTF16(kWebAppTitle);

    web_app::InstallFinalizer::FinalizeOptions options;
    options.install_source = webapps::WebappInstallSource::EXTERNAL_POLICY;

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
    extension_prefs->OnExtensionUninstalled(
        app_id, mojom::ManifestLocation::kExternalPolicy,
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
  std::unique_ptr<web_app::TestOsIntegrationManager> os_integration_manager_;
};

TEST_F(BookmarkAppInstallFinalizerTest, BasicInstallFails) {
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
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;
  bool callback_called = false;

  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kBookmarkExtensionInstallError,
                  code);
        EXPECT_TRUE(installed_app_id.empty());
        callback_called = true;
        run_loop.Quit();
      }));

  fake_crx_installer->WaitForInstallToTrigger();
  fake_crx_installer->SimulateInstallFailed();
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallFinalizerTest, DefaultInstalledSucceeds) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::EXTERNAL_DEFAULT;

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
        EXPECT_EQ(mojom::ManifestLocation::kExternalPrefDownload,
                  extension->location());
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
  options.install_source = webapps::WebappInstallSource::EXTERNAL_POLICY;

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

TEST_F(BookmarkAppInstallFinalizerTest, NoNetworkInstallForArc) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = WebAppUrl();

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::ARC;

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
        EXPECT_EQ(mojom::ManifestLocation::kExternalPrefDownload,
                  extension->location());

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
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;
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
