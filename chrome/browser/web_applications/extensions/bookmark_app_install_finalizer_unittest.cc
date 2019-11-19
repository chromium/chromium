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
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
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

const GURL kWebAppUrl("https://foo.example");
const GURL kAlternateWebAppUrl("https://bar.example");
const char kWebAppTitle[] = "Foo Title";

}  // namespace

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

    void OnUnpackSuccess(
        const base::FilePath& temp_dir,
        const base::FilePath& extension_dir,
        std::unique_ptr<base::DictionaryValue> original_manifest,
        const Extension* extension,
        const SkBitmap& install_icon,
        const base::Optional<int>& dnr_ruleset_checksum) override {
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
    ui_manager_ = std::make_unique<web_app::TestWebAppUiManager>();

    finalizer_ = std::make_unique<BookmarkAppInstallFinalizer>(profile());
    finalizer_->SetSubsystems(registrar_.get(), ui_manager_.get());
  }

  web_app::AppId InstallExternalApp(const GURL& app_url) {
    auto info = std::make_unique<WebApplicationInfo>();
    info->app_url = app_url;
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
        .Insert(app_url, app_id,
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
  std::unique_ptr<web_app::TestWebAppUiManager> ui_manager_;
  std::unique_ptr<BookmarkAppInstallFinalizer> finalizer_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallFinalizerTest);
};

TEST_F(BookmarkAppInstallFinalizerTest, BasicInstallSucceeds) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->app_url = kWebAppUrl;
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  base::RunLoop run_loop;
  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::INTERNAL_DEFAULT;
  web_app::AppId app_id;
  bool callback_called = false;

  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
        app_id = installed_app_id;
        callback_called = true;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

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
  info->app_url = kWebAppUrl;
  info->title = base::ASCIIToUTF16(kWebAppTitle);

  base::RunLoop run_loop;
  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::INTERNAL_DEFAULT;
  bool callback_called = false;

  finalizer().FinalizeInstall(
      *info, options,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kFailedUnknownReason, code);
        EXPECT_TRUE(installed_app_id.empty());
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
    web_application_info.app_url = url1;

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
    web_application_info.app_url = url2;

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
  info->app_url = kWebAppUrl;
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
  info->app_url = kWebAppUrl;
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
  auto info = std::make_unique<WebApplicationInfo>();
  info->app_url = kWebAppUrl;
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

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, NoNetworkInstallForArc) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->app_url = kWebAppUrl;

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
  InstallExternalApp(kWebAppUrl);
  ASSERT_EQ(1u, enabled_extensions().size());

  base::RunLoop run_loop;
  finalizer().UninstallExternalWebApp(
      kWebAppUrl, web_app::ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_TRUE(uninstalled);
        EXPECT_EQ(0u, enabled_extensions().size());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest, UninstallExternalWebApp_Multiple) {
  auto foo_app_id = InstallExternalApp(kWebAppUrl);
  auto bar_app_id = InstallExternalApp(kAlternateWebAppUrl);
  ASSERT_EQ(2u, enabled_extensions().size());

  // Uninstall one app.
  {
    base::RunLoop run_loop;
    finalizer().UninstallExternalWebApp(
        kWebAppUrl, web_app::ExternalInstallSource::kExternalPolicy,
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
    finalizer().UninstallExternalWebApp(
        kAlternateWebAppUrl, web_app::ExternalInstallSource::kExternalPolicy,
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
  auto app_id = InstallExternalApp(kWebAppUrl);
  SimulateExternalAppUninstalledByUser(app_id);

  base::RunLoop run_loop;
  finalizer().UninstallExternalWebApp(
      kWebAppUrl, web_app::ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_FALSE(uninstalled);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest,
       UninstallExternalWebApp_FailsNeverInstalled) {
  base::RunLoop run_loop;
  finalizer().UninstallExternalWebApp(
      kWebAppUrl, web_app::ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_FALSE(uninstalled);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallFinalizerTest,
       UninstallExternalWebApp_FailsAlreadyUninstalled) {
  InstallExternalApp(kWebAppUrl);

  // Uninstall the app.
  {
    base::RunLoop run_loop;
    finalizer().UninstallExternalWebApp(
        kWebAppUrl, web_app::ExternalInstallSource::kExternalPolicy,
        base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Try to uninstall it again.
  {
    base::RunLoop run_loop;
    finalizer().UninstallExternalWebApp(
        kWebAppUrl, web_app::ExternalInstallSource::kExternalPolicy,
        base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_FALSE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(BookmarkAppInstallFinalizerTest, NotLocallyInstalled) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->app_url = kWebAppUrl;

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
