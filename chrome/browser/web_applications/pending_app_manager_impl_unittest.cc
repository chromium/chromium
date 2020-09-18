// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/pending_app_manager_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/one_shot_event.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/pending_app_install_task.h"
#include "chrome/browser/web_applications/pending_app_registration_task.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_registration_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

using InstallAppsResults = std::vector<std::pair<GURL, InstallResultCode>>;
using UninstallAppsResults = std::vector<std::pair<GURL, bool>>;

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL FooWebAppUrl() {
  return GURL("https://foo.example");
}
GURL BarWebAppUrl() {
  return GURL("https://bar.example");
}
GURL QuxWebAppUrl() {
  return GURL("https://qux.example");
}

GURL FooLaunchUrl() {
  return GURL("https://foo.example/launch");
}
GURL BarLaunchUrl() {
  return GURL("https://bar.example/launch");
}
GURL QuxLaunchUrl() {
  return GURL("https://qux.example/launch");
}

ExternalInstallOptions GetFooInstallOptions(
    base::Optional<bool> override_previous_user_uninstall =
        base::Optional<bool>()) {
  ExternalInstallOptions options(FooWebAppUrl(), DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);

  if (override_previous_user_uninstall.has_value())
    options.override_previous_user_uninstall =
        *override_previous_user_uninstall;

  return options;
}

ExternalInstallOptions GetBarInstallOptions() {
  ExternalInstallOptions options(BarWebAppUrl(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  return options;
}

ExternalInstallOptions GetQuxInstallOptions() {
  ExternalInstallOptions options(QuxWebAppUrl(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  return options;
}

std::unique_ptr<WebApplicationInfo> GetFooWebApplicationInfo() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = FooWebAppUrl();
  info->scope = FooWebAppUrl().GetWithoutFilename();
  info->title = base::UTF8ToUTF16("Foo Web App");
  return info;
}

ExternalInstallOptions GetFooInstallOptionsWithWebAppInfo(
    base::Optional<bool> override_previous_user_uninstall =
        base::Optional<bool>()) {
  ExternalInstallOptions options(FooWebAppUrl(), DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating(&GetFooWebApplicationInfo);

  if (override_previous_user_uninstall.has_value())
    options.override_previous_user_uninstall =
        *override_previous_user_uninstall;

  return options;
}

std::string GenerateFakeAppId(const GURL& url) {
  return TestInstallFinalizer::GetAppIdForUrl(url);
}

class TestPendingAppManagerImpl : public PendingAppManagerImpl {
 public:
  TestPendingAppManagerImpl(Profile* profile,
                            TestAppRegistrar* test_app_registrar)
      : PendingAppManagerImpl(profile),
        test_app_registrar_(test_app_registrar) {}

  ~TestPendingAppManagerImpl() override {
    DCHECK(next_installation_task_results_.empty());
    DCHECK(next_installation_launch_urls_.empty());
    DCHECK(!preempt_registration_callback_);
  }

  size_t install_run_count() { return install_run_count_; }

  const std::vector<ExternalInstallOptions>& install_options_list() {
    return install_options_list_;
  }

  size_t registration_run_count() { return registration_run_count_; }

  const GURL& last_registered_install_url() {
    return last_registered_install_url_;
  }

  void SetNextInstallationTaskResult(const GURL& app_url,
                                     InstallResultCode result_code) {
    DCHECK(!base::Contains(next_installation_task_results_, app_url));
    next_installation_task_results_[app_url] = result_code;
  }

  void SetNextInstallationLaunchURL(const GURL& app_url,
                                    const GURL& launch_url) {
    DCHECK(!base::Contains(next_installation_launch_urls_, app_url));
    next_installation_launch_urls_[app_url] = launch_url;
  }

  bool MaybePreemptRegistration() {
    if (!preempt_registration_callback_)
      return false;

    base::Optional<base::OnceClosure> callback;
    preempt_registration_callback_.swap(callback);
    std::move(*callback).Run();
    return true;
  }

  std::unique_ptr<PendingAppInstallTask> CreateInstallationTask(
      ExternalInstallOptions install_options) override {
    return std::make_unique<TestPendingAppInstallTask>(
        this, profile(), std::move(install_options));
  }

  std::unique_ptr<PendingAppRegistrationTaskBase> StartRegistration(
      GURL install_url) override {
    ++registration_run_count_;
    last_registered_install_url_ = install_url;
    return std::make_unique<TestPendingAppRegistrationTask>(install_url, this);
  }

  void OnInstallCalled(const ExternalInstallOptions& install_options) {
    ++install_run_count_;
    install_options_list_.push_back(install_options);
  }

  InstallResultCode GetNextInstallationTaskResult(const GURL& url) {
    DCHECK(base::Contains(next_installation_task_results_, url));
    auto result = next_installation_task_results_.at(url);
    next_installation_task_results_.erase(url);
    return result;
  }

  GURL GetNextInstallationLaunchURL(const GURL& url) {
    if (!base::Contains(next_installation_launch_urls_, url))
      return GURL::EmptyGURL();

    auto result = next_installation_launch_urls_.at(url);
    next_installation_launch_urls_.erase(url);
    return result;
  }

  void WaitForRegistrationAndCancel() {
    DCHECK(!preempt_registration_callback_);

    base::RunLoop run_loop;
    preempt_registration_callback_ =
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); });
    run_loop.Run();
  }

  void ReleaseWebContents() override {
    PendingAppManagerImpl::ReleaseWebContents();

    // May be called more than once, if PendingAppManager finishes all tasks
    // before it is shutdown.
    if (!web_contents_released_event_.is_signaled()) {
      web_contents_released_event_.Signal();
    }
  }

  // Wait for PendingAppManager to finish all installations and registrations.
  void WaitForWebContentsReleased() {
    base::RunLoop run_loop;
    web_contents_released_event_.Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  TestAppRegistrar* registrar() { return test_app_registrar_; }

 private:
  class TestPendingAppInstallTask : public PendingAppInstallTask {
   public:
    TestPendingAppInstallTask(
        TestPendingAppManagerImpl* pending_app_manager_impl,
        Profile* profile,
        ExternalInstallOptions install_options)
        : PendingAppInstallTask(
              profile,
              pending_app_manager_impl->registrar(),
              pending_app_manager_impl->os_integration_manager(),
              pending_app_manager_impl->ui_manager(),
              pending_app_manager_impl->finalizer(),
              pending_app_manager_impl->install_manager(),
              install_options),
          pending_app_manager_impl_(pending_app_manager_impl),
          externally_installed_app_prefs_(profile->GetPrefs()) {}
    ~TestPendingAppInstallTask() override = default;

    void DoInstall(const GURL& install_url,
                   ExternalInstallSource install_source,
                   bool is_placeholder,
                   ResultCallback callback) {
      auto result_code =
          pending_app_manager_impl_->GetNextInstallationTaskResult(install_url);
      base::Optional<AppId> app_id;
      if (result_code == InstallResultCode::kSuccessNewInstall) {
        app_id = GenerateFakeAppId(install_url);
        GURL launch_url =
            pending_app_manager_impl_->GetNextInstallationLaunchURL(
                install_url);
        pending_app_manager_impl_->registrar()->AddExternalApp(
            *app_id, {install_url, install_source, launch_url});
        externally_installed_app_prefs_.Insert(install_url, *app_id,
                                               install_source);
        externally_installed_app_prefs_.SetIsPlaceholder(install_url,
                                                         is_placeholder);
      }
      std::move(callback).Run({result_code, app_id});
    }
    void Install(content::WebContents* web_contents,
                 WebAppUrlLoader::Result url_loaded_result,
                 ResultCallback callback) override {
      pending_app_manager_impl_->OnInstallCalled(install_options());

      const GURL& install_url = install_options().install_url;
      DoInstall(install_url, install_options().install_source,
                (url_loaded_result != WebAppUrlLoader::Result::kUrlLoaded),
                std::move(callback));
    }

    void InstallFromInfo(ResultCallback callback) override {
      pending_app_manager_impl_->OnInstallCalled(install_options());

      GURL install_url = install_options().app_info_factory.Run()->start_url;
      DoInstall(install_url, install_options().install_source, false,
                std::move(callback));
    }

   private:
    TestPendingAppManagerImpl* pending_app_manager_impl_;
    ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

    DISALLOW_COPY_AND_ASSIGN(TestPendingAppInstallTask);
  };

  class TestPendingAppRegistrationTask : public PendingAppRegistrationTaskBase {
   public:
    TestPendingAppRegistrationTask(
        const GURL& install_url,
        TestPendingAppManagerImpl* pending_app_manager_impl)
        : PendingAppRegistrationTaskBase(install_url),
          pending_app_manager_impl_(pending_app_manager_impl) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&TestPendingAppRegistrationTask::OnProgress,
                         weak_ptr_factory_.GetWeakPtr(), install_url));
    }
    ~TestPendingAppRegistrationTask() override = default;

   private:
    void OnProgress(GURL install_url) {
      if (pending_app_manager_impl_->MaybePreemptRegistration())
        return;
      pending_app_manager_impl_->OnRegistrationFinished(
          install_url, RegistrationResultCode::kSuccess);
    }

    TestPendingAppManagerImpl* const pending_app_manager_impl_;

    base::WeakPtrFactory<TestPendingAppRegistrationTask> weak_ptr_factory_{
        this};

    DISALLOW_COPY_AND_ASSIGN(TestPendingAppRegistrationTask);
  };

  TestAppRegistrar* test_app_registrar_;

  std::vector<ExternalInstallOptions> install_options_list_;
  GURL last_registered_install_url_;
  size_t install_run_count_ = 0;
  size_t registration_run_count_ = 0;

  std::map<GURL, InstallResultCode> next_installation_task_results_;
  std::map<GURL, GURL> next_installation_launch_urls_;
  base::Optional<base::OnceClosure> preempt_registration_callback_;
  base::OneShotEvent web_contents_released_event_;
};

}  // namespace

class PendingAppManagerImplTest
    : public ChromeRenderViewHostTestHarness,
      public ::testing::WithParamInterface<ProviderType> {
 public:
  PendingAppManagerImplTest() {
    if (GetParam() == web_app::ProviderType::kWebApps) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsWithoutExtensions);
    } else if (GetParam() == web_app::ProviderType::kBookmarkApps) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsWithoutExtensions);
    }
  }

  ~PendingAppManagerImplTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    auto* provider = TestWebAppProvider::Get(profile());

    auto test_app_registrar = std::make_unique<TestAppRegistrar>();
    app_registrar_ = test_app_registrar.get();
    provider->SetRegistrar(std::move(test_app_registrar));

    auto test_pending_app_manager =
        std::make_unique<TestPendingAppManagerImpl>(profile(), app_registrar_);
    pending_app_manager_impl_ = test_pending_app_manager.get();
    provider->SetPendingAppManager(std::move(test_pending_app_manager));

    auto url_loader = std::make_unique<TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();
    pending_app_manager_impl_->SetUrlLoaderForTesting(std::move(url_loader));

    auto test_install_finalizer = std::make_unique<TestInstallFinalizer>();
    install_finalizer_ = test_install_finalizer.get();
    provider->SetInstallFinalizer(std::move(test_install_finalizer));

    auto ui_manager = std::make_unique<TestWebAppUiManager>();
    ui_manager_ = ui_manager.get();
    provider->SetWebAppUiManager(std::move(ui_manager));

    provider->Start();
  }

 protected:
  std::pair<GURL, InstallResultCode> InstallAndWait(
      PendingAppManager* pending_app_manager,
      ExternalInstallOptions install_options) {
    base::RunLoop run_loop;

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;

    pending_app_manager_impl()->Install(
        std::move(install_options),
        base::BindLambdaForTesting([&](const GURL& u, InstallResultCode c) {
          url = u;
          code = c;
          run_loop.Quit();
        }));
    run_loop.Run();

    return {url.value(), code.value()};
  }

  std::vector<std::pair<GURL, InstallResultCode>> InstallAppsAndWait(
      PendingAppManager* pending_app_manager,
      std::vector<ExternalInstallOptions> apps_to_install) {
    std::vector<std::pair<GURL, InstallResultCode>> results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_install.size(), run_loop.QuitClosure());
    pending_app_manager_impl()->InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url, InstallResultCode code) {
              results.emplace_back(url, code);
              barrier_closure.Run();
            }));
    run_loop.Run();

    return results;
  }

  std::vector<std::pair<GURL, bool>> UninstallAppsAndWait(
      PendingAppManager* pending_app_manager,
      ExternalInstallSource install_source,
      std::vector<GURL> apps_to_uninstall) {
    std::vector<std::pair<GURL, bool>> results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_uninstall.size(), run_loop.QuitClosure());
    pending_app_manager->UninstallApps(
        std::move(apps_to_uninstall), install_source,
        base::BindLambdaForTesting(
            [&](const GURL& url, bool successfully_uninstalled) {
              results.emplace_back(url, successfully_uninstalled);
              barrier_closure.Run();
            }));
    run_loop.Run();

    return results;
  }

  // ExternalInstallOptions that was used to run the last installation task.
  const ExternalInstallOptions& last_install_options() {
    DCHECK(!pending_app_manager_impl_->install_options_list().empty());
    return pending_app_manager_impl_->install_options_list().back();
  }

  // Number of times PendingAppInstallTask::Install was called. Reflects
  // how many times we've tried to create a web app.
  size_t install_run_count() {
    return pending_app_manager_impl_->install_run_count();
  }

  // Number of times PendingAppManagerImpl::StartRegistration was called.
  // Reflects how many times we've tried to cache service worker resources
  // for a web app.
  size_t registration_run_count() {
    return pending_app_manager_impl_->registration_run_count();
  }

  size_t uninstall_call_count() {
    return install_finalizer_->uninstall_external_web_app_urls().size();
  }

  const std::vector<GURL>& uninstalled_app_urls() {
    return install_finalizer_->uninstall_external_web_app_urls();
  }

  const GURL& last_registered_install_url() {
    return pending_app_manager_impl_->last_registered_install_url();
  }

  const GURL& last_uninstalled_app_url() {
    return install_finalizer_->uninstall_external_web_app_urls().back();
  }

  TestPendingAppManagerImpl* pending_app_manager_impl() {
    return pending_app_manager_impl_;
  }

  TestAppRegistrar* registrar() { return app_registrar_; }

  TestWebAppUiManager* ui_manager() { return ui_manager_; }

  TestWebAppUrlLoader* url_loader() { return url_loader_; }

  TestInstallFinalizer* install_finalizer() { return install_finalizer_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestAppRegistrar* app_registrar_ = nullptr;
  TestPendingAppManagerImpl* pending_app_manager_impl_ = nullptr;
  TestInstallFinalizer* install_finalizer_ = nullptr;
  TestWebAppUiManager* ui_manager_ = nullptr;
  TestWebAppUrlLoader* url_loader_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PendingAppManagerImplTest);
};

TEST_P(PendingAppManagerImplTest, Install_Succeeds) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  pending_app_manager_impl()->SetNextInstallationLaunchURL(FooWebAppUrl(),
                                                           FooLaunchUrl());
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  base::Optional<GURL> url;
  base::Optional<InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  EXPECT_EQ(FooWebAppUrl(), url.value());

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetFooInstallOptions(), last_install_options());

  WebAppRegistrationWaiter(pending_app_manager_impl())
      .AwaitNextRegistration(FooWebAppUrl(), RegistrationResultCode::kSuccess);
  EXPECT_EQ(1U, registration_run_count());
  EXPECT_EQ(FooWebAppUrl(), last_registered_install_url());
}

TEST_P(PendingAppManagerImplTest, Install_SerialCallsDifferentApps) {
  // Load about:blanks twice in total, once for each install.
  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                          WebAppUrlLoader::Result::kUrlLoaded});

  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  pending_app_manager_impl()->SetNextInstallationLaunchURL(FooWebAppUrl(),
                                                           FooLaunchUrl());
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(FooWebAppUrl(), url.value());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetFooInstallOptions(), last_install_options());
  }

  pending_app_manager_impl()->WaitForRegistrationAndCancel();
  // FooLaunchUrl() registration will be attempted again after
  // BarWebAppUrl() installs.

  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  pending_app_manager_impl()->SetNextInstallationLaunchURL(BarWebAppUrl(),
                                                           BarLaunchUrl());
  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;

    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetBarInstallOptions());

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(BarWebAppUrl(), url.value());

    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(GetBarInstallOptions(), last_install_options());
  }

  WebAppRegistrationWaiter(pending_app_manager_impl())
      .AwaitNextRegistration(FooWebAppUrl(), RegistrationResultCode::kSuccess);
  WebAppRegistrationWaiter(pending_app_manager_impl())
      .AwaitNextRegistration(BarWebAppUrl(), RegistrationResultCode::kSuccess);
  EXPECT_EQ(3U, registration_run_count());
  EXPECT_EQ(BarWebAppUrl(), last_registered_install_url());
}

TEST_P(PendingAppManagerImplTest, Install_ConcurrentCallsDifferentApps) {
  // Load about:blanks twice in total, once for each install.
  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                          WebAppUrlLoader::Result::kUrlLoaded});

  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(FooWebAppUrl(), url);

        // Two installations tasks should have run at this point,
        // one from the last call to install (which gets higher priority),
        // and another one for this call to install.
        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetFooInstallOptions(), last_install_options());

        run_loop.Quit();
      }));
  pending_app_manager_impl()->Install(
      GetBarInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(BarWebAppUrl(), url);

        // The last call gets higher priority so only one
        // installation task should have run at this point.
        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetBarInstallOptions(), last_install_options());
      }));
  run_loop.Run();
}

TEST_P(PendingAppManagerImplTest, Install_PendingSuccessfulTask) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SaveLoadUrlRequests();

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(FooWebAppUrl(), url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptions(), last_install_options());

        foo_run_loop.Quit();
      }));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
  pending_app_manager_impl()->Install(
      GetBarInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(BarWebAppUrl(), url);

        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetBarInstallOptions(), last_install_options());

        bar_run_loop.Quit();
      }));

  url_loader()->ProcessLoadUrlRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
  url_loader()->ProcessLoadUrlRequests();
  bar_run_loop.Run();
}

TEST_P(PendingAppManagerImplTest, InstallWithWebAppInfo_Succeeds) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);

  base::RunLoop foo_run_loop;

  pending_app_manager_impl()->Install(
      GetFooInstallOptionsWithWebAppInfo(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(FooWebAppUrl(), url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptionsWithWebAppInfo(), last_install_options());

        foo_run_loop.Quit();
      }));
  // Make sure the installation has started.
  foo_run_loop.Run();
}

TEST_P(PendingAppManagerImplTest, InstallAppsWithWebAppInfoAndUrl_Multiple) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);

  // Load about:blanks twice in total, once for each install.
  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});

  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptionsWithWebAppInfo());
  apps_to_install.push_back(GetBarInstallOptions());

  InstallAppsResults results = InstallAppsAndWait(pending_app_manager_impl(),
                                                  std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{FooWebAppUrl(), InstallResultCode::kSuccessNewInstall},
                 {BarWebAppUrl(), InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(2u, install_run_count());
  EXPECT_EQ(GetBarInstallOptions(), last_install_options());
}

TEST_P(PendingAppManagerImplTest, InstallWithWebAppInfo_Succeeds_Twice) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  pending_app_manager_impl()->Install(
      GetFooInstallOptionsWithWebAppInfo(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(FooWebAppUrl(), url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptionsWithWebAppInfo(), last_install_options());
        foo_run_loop.Quit();
      }));

  base::RunLoop().RunUntilIdle();

  pending_app_manager_impl()->Install(
      GetFooInstallOptionsWithWebAppInfo(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, code);
        EXPECT_EQ(FooWebAppUrl(), url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptionsWithWebAppInfo(), last_install_options());

        bar_run_loop.Quit();
      }));
  foo_run_loop.Run();
  base::RunLoop().RunUntilIdle();
  bar_run_loop.Run();
}

TEST_P(PendingAppManagerImplTest, Install_PendingFailingTask) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kWebAppDisabled);
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  url_loader()->SaveLoadUrlRequests();

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  url_loader()->SetPrepareForLoadResultLoaded();
  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kWebAppDisabled, code);
        EXPECT_EQ(FooWebAppUrl(), url);

        EXPECT_EQ(1u, install_run_count());

        foo_run_loop.Quit();
      }));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  pending_app_manager_impl()->Install(
      GetBarInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(BarWebAppUrl(), url);

        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetBarInstallOptions(), last_install_options());

        bar_run_loop.Quit();
      }));

  url_loader()->ProcessLoadUrlRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->ProcessLoadUrlRequests();
  bar_run_loop.Run();
}

TEST_P(PendingAppManagerImplTest, Install_ReentrantCallback) {
  // Load about:blanks twice in total, once for each install.
  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                          WebAppUrlLoader::Result::kUrlLoaded});

  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  auto final_callback =
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(BarWebAppUrl(), url);

        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetBarInstallOptions(), last_install_options());
        run_loop.Quit();
      });
  auto reentrant_callback =
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(FooWebAppUrl(), url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptions(), last_install_options());

        pending_app_manager_impl()->Install(GetBarInstallOptions(),
                                            final_callback);
      });

  // Call Install() with a callback that tries to install another app.
  pending_app_manager_impl()->Install(GetFooInstallOptions(),
                                      reentrant_callback);
  run_loop.Run();
}

TEST_P(PendingAppManagerImplTest, Install_SerialCallsSameApp) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(FooWebAppUrl(), url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetFooInstallOptions(), last_install_options());
  }

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, code);
    EXPECT_EQ(FooWebAppUrl(), url);

    // The app is already installed so we shouldn't try to install it again.
    EXPECT_EQ(1u, install_run_count());
  }
}

TEST_P(PendingAppManagerImplTest, Install_ConcurrentCallsSameApp) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  bool first_callback_ran = false;

  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        // kSuccessAlreadyInstalled because the last call to Install gets higher
        // priority.
        EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, code);
        EXPECT_EQ(FooWebAppUrl(), url);

        // Only one installation task should run because the app was already
        // installed.
        EXPECT_EQ(1u, install_run_count());

        EXPECT_TRUE(first_callback_ran);

        run_loop.Quit();
      }));

  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(FooWebAppUrl(), url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetFooInstallOptions(), last_install_options());
        first_callback_ran = true;
      }));
  run_loop.Run();

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetFooInstallOptions(), last_install_options());
}

TEST_P(PendingAppManagerImplTest, Install_AlwaysUpdate) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  auto get_force_reinstall_info = []() {
    ExternalInstallOptions options(FooWebAppUrl(), DisplayMode::kStandalone,
                                   ExternalInstallSource::kExternalPolicy);
    options.force_reinstall = true;
    return options;
  };

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), get_force_reinstall_info());

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(FooWebAppUrl(), url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(get_force_reinstall_info(), last_install_options());
  }

  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), get_force_reinstall_info());

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(FooWebAppUrl(), url);

    // The app should be installed again because of the |force_reinstall| flag.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(get_force_reinstall_info(), last_install_options());
  }
}

TEST_P(PendingAppManagerImplTest, Install_InstallationFails) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kWebAppDisabled);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::Optional<GURL> url;
  base::Optional<InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

  EXPECT_EQ(InstallResultCode::kWebAppDisabled, code);
  EXPECT_EQ(FooWebAppUrl(), url);

  EXPECT_EQ(1u, install_run_count());
}

TEST_P(PendingAppManagerImplTest, Install_PlaceholderApp) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(
      FooWebAppUrl(), WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  base::Optional<GURL> url;
  base::Optional<InstallResultCode> code;
  std::tie(url, code) =
      InstallAndWait(pending_app_manager_impl(), install_options);

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
  EXPECT_EQ(FooWebAppUrl(), url);

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(install_options, last_install_options());
}

TEST_P(PendingAppManagerImplTest, InstallApps_Succeeds) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());

  InstallAppsResults results = InstallAppsAndWait(pending_app_manager_impl(),
                                                  std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{FooWebAppUrl(), InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetFooInstallOptions(), last_install_options());
}

TEST_P(PendingAppManagerImplTest, InstallApps_FailsInstallationFails) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kWebAppDisabled);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());

  InstallAppsResults results = InstallAppsAndWait(pending_app_manager_impl(),
                                                  std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{FooWebAppUrl(), InstallResultCode::kWebAppDisabled}}));

  EXPECT_EQ(1u, install_run_count());
}

TEST_P(PendingAppManagerImplTest, InstallApps_PlaceholderApp) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(
      FooWebAppUrl(), WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;
  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(install_options);

  InstallAppsResults results = InstallAppsAndWait(pending_app_manager_impl(),
                                                  std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{FooWebAppUrl(), InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(install_options, last_install_options());
}

TEST_P(PendingAppManagerImplTest, InstallApps_Multiple) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);

  // Load about:blanks twice in total, once for each install.
  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                          WebAppUrlLoader::Result::kUrlLoaded});

  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.push_back(GetBarInstallOptions());

  InstallAppsResults results = InstallAppsAndWait(pending_app_manager_impl(),
                                                  std::move(apps_to_install));

  EXPECT_EQ(results,
            InstallAppsResults(
                {{FooWebAppUrl(), InstallResultCode::kSuccessNewInstall},
                 {BarWebAppUrl(), InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(2u, install_run_count());
  EXPECT_EQ(GetBarInstallOptions(), last_install_options());
}

TEST_P(PendingAppManagerImplTest, InstallApps_PendingInstallApps) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  // Load about:blanks twice in total, once for each install.
  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                          WebAppUrlLoader::Result::kUrlLoaded});
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  {
    std::vector<ExternalInstallOptions> apps_to_install;
    apps_to_install.push_back(GetFooInstallOptions());

    pending_app_manager_impl()->InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url, InstallResultCode code) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
              EXPECT_EQ(FooWebAppUrl(), url);

              EXPECT_EQ(1u, install_run_count());
              EXPECT_EQ(GetFooInstallOptions(), last_install_options());
            }));
  }

  {
    std::vector<ExternalInstallOptions> apps_to_install;
    apps_to_install.push_back(GetBarInstallOptions());

    pending_app_manager_impl()->InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url, InstallResultCode code) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
              EXPECT_EQ(BarWebAppUrl(), url);

              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetBarInstallOptions(), last_install_options());

              run_loop.Quit();
            }));
  }
  run_loop.Run();
}

TEST_P(PendingAppManagerImplTest, Install_PendingMultipleInstallApps) {
  // Load about:blanks three times in total, once for each install.
  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                          WebAppUrlLoader::Result::kUrlLoaded,
                                          WebAppUrlLoader::Result::kUrlLoaded});
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  pending_app_manager_impl()->SetNextInstallationLaunchURL(FooWebAppUrl(),
                                                           FooLaunchUrl());
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  pending_app_manager_impl()->SetNextInstallationLaunchURL(BarWebAppUrl(),
                                                           BarLaunchUrl());
  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      QuxWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  pending_app_manager_impl()->SetNextInstallationLaunchURL(QuxWebAppUrl(),
                                                           QuxLaunchUrl());
  url_loader()->SetNextLoadUrlResult(QuxWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.push_back(GetBarInstallOptions());

  // Queue through InstallApps.
  int callback_calls = 0;
  pending_app_manager_impl()->InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        ++callback_calls;
        if (callback_calls == 1) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(FooWebAppUrl(), url);

          EXPECT_EQ(2u, install_run_count());
          EXPECT_EQ(GetFooInstallOptions(), last_install_options());
        } else if (callback_calls == 2) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(BarWebAppUrl(), url);

          EXPECT_EQ(3u, install_run_count());
          EXPECT_EQ(GetBarInstallOptions(), last_install_options());
        } else {
          NOTREACHED();
        }
      }));

  // Queue through Install.
  pending_app_manager_impl()->Install(
      GetQuxInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(QuxWebAppUrl(), url);

        // The install request from Install should be processed first.
        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetQuxInstallOptions(), last_install_options());
      }));

  WebAppRegistrationWaiter(pending_app_manager_impl())
      .AwaitNextRegistration(QuxWebAppUrl(), RegistrationResultCode::kSuccess);
  WebAppRegistrationWaiter(pending_app_manager_impl())
      .AwaitNextRegistration(FooWebAppUrl(), RegistrationResultCode::kSuccess);
  WebAppRegistrationWaiter(pending_app_manager_impl())
      .AwaitNextRegistration(BarWebAppUrl(), RegistrationResultCode::kSuccess);
  EXPECT_EQ(3U, registration_run_count());
  EXPECT_EQ(BarWebAppUrl(), last_registered_install_url());
}

TEST_P(PendingAppManagerImplTest, InstallApps_PendingInstall) {
  url_loader()->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                          WebAppUrlLoader::Result::kUrlLoaded,
                                          WebAppUrlLoader::Result::kUrlLoaded});
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      BarWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(BarWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      QuxWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetNextLoadUrlResult(QuxWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  // Queue through Install.
  pending_app_manager_impl()->Install(
      GetQuxInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(QuxWebAppUrl(), url);

        // The install request from Install should be processed first.
        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetQuxInstallOptions(), last_install_options());
      }));

  // Queue through InstallApps.
  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetFooInstallOptions());
  apps_to_install.push_back(GetBarInstallOptions());

  int callback_calls = 0;
  pending_app_manager_impl()->InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        ++callback_calls;
        if (callback_calls == 1) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(FooWebAppUrl(), url);

          // The install requests from InstallApps should be processed next.
          EXPECT_EQ(2u, install_run_count());
          EXPECT_EQ(GetFooInstallOptions(), last_install_options());

          return;
        }
        if (callback_calls == 2) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(BarWebAppUrl(), url);

          EXPECT_EQ(3u, install_run_count());
          EXPECT_EQ(GetBarInstallOptions(), last_install_options());

          run_loop.Quit();
          return;
        }
        NOTREACHED();
      }));
  run_loop.Run();
}

TEST_P(PendingAppManagerImplTest, AppUninstalled) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  }

  // Simulate the app getting uninstalled.
  registrar()->RemoveExternalAppByInstallUrl(FooWebAppUrl());

  // Try to install the app again.
  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    url_loader()->SetNextLoadUrlResult(GURL(url::kAboutBlankURL),
                                       WebAppUrlLoader::Result::kUrlLoaded);
    url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                       WebAppUrlLoader::Result::kUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    // The app was uninstalled so a new installation task should run.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  }
}

TEST_P(PendingAppManagerImplTest, ExternalAppUninstalled) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), GetFooInstallOptions());

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  }

  // Simulate external app for the app getting uninstalled by the user.
  const std::string app_id = GenerateFakeAppId(FooWebAppUrl());
  install_finalizer()->SimulateExternalAppUninstalledByUser(app_id);
  if (registrar()->IsInstalled(app_id))
    registrar()->RemoveExternalApp(app_id);

  // The app was uninstalled by the user. Installing again should succeed
  // or fail depending on whether we set override_previous_user_uninstall. We
  // try with override_previous_user_uninstall false first, true second.
  {
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(
        pending_app_manager_impl(),
        GetFooInstallOptions(false /* override_previous_user_uninstall */));

    // The app shouldn't be installed because the user previously uninstalled
    // it, so there shouldn't be any new installation task runs.
    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(InstallResultCode::kPreviouslyUninstalled, code.value());
  }

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                       WebAppUrlLoader::Result::kUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) = InstallAndWait(
        pending_app_manager_impl(),
        GetFooInstallOptions(true /* override_previous_user_uninstall */));

    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
  }
}

TEST_P(PendingAppManagerImplTest, UninstallApps_Succeeds) {
  registrar()->AddExternalApp(
      GenerateFakeAppId(FooWebAppUrl()),
      {FooWebAppUrl(), ExternalInstallSource::kExternalPolicy});

  install_finalizer()->SetNextUninstallExternalWebAppResult(FooWebAppUrl(),
                                                            true);
  UninstallAppsResults results = UninstallAppsAndWait(
      pending_app_manager_impl(), ExternalInstallSource::kExternalPolicy,
      std::vector<GURL>{FooWebAppUrl()});

  EXPECT_EQ(results, UninstallAppsResults({{FooWebAppUrl(), true}}));

  EXPECT_EQ(1u, uninstall_call_count());
  EXPECT_EQ(FooWebAppUrl(), last_uninstalled_app_url());
}

TEST_P(PendingAppManagerImplTest, UninstallApps_Fails) {
  install_finalizer()->SetNextUninstallExternalWebAppResult(FooWebAppUrl(),
                                                            false);
  UninstallAppsResults results = UninstallAppsAndWait(
      pending_app_manager_impl(), ExternalInstallSource::kExternalPolicy,
      std::vector<GURL>{FooWebAppUrl()});
  EXPECT_EQ(results, UninstallAppsResults({{FooWebAppUrl(), false}}));

  EXPECT_EQ(1u, uninstall_call_count());
  EXPECT_EQ(FooWebAppUrl(), last_uninstalled_app_url());
}

TEST_P(PendingAppManagerImplTest, UninstallApps_Multiple) {
  registrar()->AddExternalApp(
      GenerateFakeAppId(FooWebAppUrl()),
      {FooWebAppUrl(), ExternalInstallSource::kExternalPolicy});
  registrar()->AddExternalApp(
      GenerateFakeAppId(BarWebAppUrl()),
      {FooWebAppUrl(), ExternalInstallSource::kExternalPolicy});

  install_finalizer()->SetNextUninstallExternalWebAppResult(FooWebAppUrl(),
                                                            true);
  install_finalizer()->SetNextUninstallExternalWebAppResult(BarWebAppUrl(),
                                                            true);
  UninstallAppsResults results = UninstallAppsAndWait(
      pending_app_manager_impl(), ExternalInstallSource::kExternalPolicy,
      std::vector<GURL>{FooWebAppUrl(), BarWebAppUrl()});
  EXPECT_EQ(results, UninstallAppsResults(
                         {{FooWebAppUrl(), true}, {BarWebAppUrl(), true}}));

  EXPECT_EQ(2u, uninstall_call_count());
  EXPECT_EQ(std::vector<GURL>({FooWebAppUrl(), BarWebAppUrl()}),
            uninstalled_app_urls());
}

TEST_P(PendingAppManagerImplTest, UninstallApps_PendingInstall) {
  pending_app_manager_impl()->SetNextInstallationTaskResult(
      FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
  url_loader()->SetPrepareForLoadResultLoaded();
  url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                     WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  pending_app_manager_impl()->Install(
      GetFooInstallOptions(),
      base::BindLambdaForTesting([&](const GURL& url, InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
        EXPECT_EQ(FooWebAppUrl(), url);
        run_loop.Quit();
      }));

  install_finalizer()->SetNextUninstallExternalWebAppResult(FooWebAppUrl(),
                                                            false);
  UninstallAppsResults uninstall_results = UninstallAppsAndWait(
      pending_app_manager_impl(), ExternalInstallSource::kExternalPolicy,
      std::vector<GURL>{FooWebAppUrl()});
  EXPECT_EQ(uninstall_results, UninstallAppsResults({{FooWebAppUrl(), false}}));
  EXPECT_EQ(1u, uninstall_call_count());

  run_loop.Run();
}

TEST_P(PendingAppManagerImplTest, ReinstallPlaceholderApp_Success) {
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(
        FooWebAppUrl(), WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                       WebAppUrlLoader::Result::kUrlLoaded);
    install_finalizer()->SetNextUninstallExternalWebAppResult(FooWebAppUrl(),
                                                              true);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(FooWebAppUrl(), url.value());

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_P(PendingAppManagerImplTest,
       ReinstallPlaceholderApp_ReinstallNotPossible) {
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(
        FooWebAppUrl(), WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Try to reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(
        FooWebAppUrl(), WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(FooWebAppUrl(), url.value());

    // Even though the placeholder app is already install, we make a call to
    // InstallFinalizer. InstallFinalizer ensures we don't unnecessarily
    // install the placeholder app again.
    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_P(PendingAppManagerImplTest,
       ReinstallPlaceholderAppWhenUnused_NoOpenedWindows) {
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(
        FooWebAppUrl(), WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    install_options.wait_for_windows_closed = true;
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    ui_manager()->SetNumWindowsForApp(GenerateFakeAppId(FooWebAppUrl()), 0);
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                       WebAppUrlLoader::Result::kUrlLoaded);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(FooWebAppUrl(), url.value());

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_P(PendingAppManagerImplTest,
       ReinstallPlaceholderAppWhenUnused_OneWindowOpened) {
  // Install a placeholder app
  auto install_options = GetFooInstallOptions();
  install_options.install_placeholder = true;

  {
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(
        FooWebAppUrl(), WebAppUrlLoader::Result::kRedirectedUrlLoaded);
    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);
    ASSERT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.reinstall_placeholder = true;
    install_options.wait_for_windows_closed = true;
    pending_app_manager_impl()->SetNextInstallationTaskResult(
        FooWebAppUrl(), InstallResultCode::kSuccessNewInstall);
    ui_manager()->SetNumWindowsForApp(GenerateFakeAppId(FooWebAppUrl()), 1);
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(FooWebAppUrl(),
                                       WebAppUrlLoader::Result::kUrlLoaded);
    install_finalizer()->SetNextUninstallExternalWebAppResult(FooWebAppUrl(),
                                                              true);

    base::Optional<GURL> url;
    base::Optional<InstallResultCode> code;
    std::tie(url, code) =
        InstallAndWait(pending_app_manager_impl(), install_options);

    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code.value());
    EXPECT_EQ(FooWebAppUrl(), url.value());

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_P(PendingAppManagerImplTest, DoNotRegisterServiceWorkerForLocalApps) {
  GURL local_urls[] = {GURL("chrome://sample"),
                       GURL("chrome-untrusted://sample")};

  for (const auto& install_url : local_urls) {
    size_t prev_install_run_count = install_run_count();

    pending_app_manager_impl()->SetNextInstallationTaskResult(
        install_url, InstallResultCode::kSuccessNewInstall);
    pending_app_manager_impl()->SetNextInstallationLaunchURL(
        install_url, install_url.Resolve("launch_page"));
    url_loader()->SetPrepareForLoadResultLoaded();
    url_loader()->SetNextLoadUrlResult(install_url,
                                       WebAppUrlLoader::Result::kUrlLoaded);
    ExternalInstallOptions install_option(
        install_url, DisplayMode::kStandalone,
        ExternalInstallSource::kSystemInstalled);
    const auto& url_and_result =
        InstallAndWait(pending_app_manager_impl(), install_option);
    EXPECT_EQ(install_url, url_and_result.first);
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, url_and_result.second);

    pending_app_manager_impl()->WaitForWebContentsReleased();
    EXPECT_EQ(prev_install_run_count + 1, install_run_count());
    EXPECT_EQ(0u, registration_run_count());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PendingAppManagerImplTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

}  // namespace web_app
