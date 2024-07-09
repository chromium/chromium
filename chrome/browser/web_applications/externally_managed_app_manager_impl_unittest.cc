// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/not_fatal_until.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_install_task.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/external_app_registration_waiter.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

using InstallAppsResults =
    std::vector<std::pair<GURL, webapps::InstallResultCode>>;
using UninstallAppsResults =
    std::vector<std::pair<GURL, webapps::UninstallResultCode>>;

ExternalInstallOptions GetInstallOptions(
    const GURL& url,
    std::optional<bool> override_previous_user_uninstall =
        std::optional<bool>()) {
  ExternalInstallOptions options(url, mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);

  if (override_previous_user_uninstall.has_value())
    options.override_previous_user_uninstall =
        *override_previous_user_uninstall;

  return options;
}

std::unique_ptr<WebAppInstallInfo> GetWebAppInstallInfo(const GURL& url) {
  std::unique_ptr<WebAppInstallInfo> info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(url);
  info->scope = url.GetWithoutFilename();
  info->title = u"Foo Web App";
  return info;
}

ExternalInstallOptions GetInstallOptionsWithWebAppInfo(
    const GURL& url,
    std::optional<bool> override_previous_user_uninstall =
        std::optional<bool>()) {
  ExternalInstallOptions options(url, mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.only_use_app_info_factory = true;
  // Static to ensure re-use across multiple function calls for
  // ExternalInstallOptions equality checking.
  static WebAppInstallInfoFactory app_info_factory =
      base::BindRepeating(&GetWebAppInstallInfo, url);
  options.app_info_factory = app_info_factory;

  if (override_previous_user_uninstall.has_value())
    options.override_previous_user_uninstall =
        *override_previous_user_uninstall;

  return options;
}

// Class to delay completion of TestExternallyManagedAppInstallTasks.
//
// Tests can call into this class to tell it to save install requests and to
// trigger running the saved requests. TestExternallyManagedAppInstallTasks call
// into this class with a OnceClosure. This class then decides if the closure
// should be run immediately or if it should be saved.
class TestExternallyManagedAppInstallTaskManager {
 public:
  TestExternallyManagedAppInstallTaskManager() = default;
  TestExternallyManagedAppInstallTaskManager(
      const TestExternallyManagedAppInstallTaskManager&) = delete;
  TestExternallyManagedAppInstallTaskManager& operator=(
      const TestExternallyManagedAppInstallTaskManager&) = delete;
  ~TestExternallyManagedAppInstallTaskManager() = default;

  void SaveInstallRequests() { should_save_requests_ = true; }

  void RunOrSaveRequest(base::OnceClosure install_request) {
    if (should_save_requests_) {
      pending_install_requests_.push(std::move(install_request));
      return;
    }
    // Post a task to simulate tasks completing asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(install_request));
  }

  void ProcessSavedRequests() {
    // Swap to avoid re-entrancy issues.
    std::queue<base::OnceClosure> pending_install_requests;
    pending_install_requests.swap(pending_install_requests_);
    while (!pending_install_requests.empty()) {
      base::OnceClosure request = std::move(pending_install_requests.front());
      pending_install_requests.pop();
      std::move(request).Run();
    }
  }

  size_t num_pending_tasks() { return pending_install_requests_.size(); }

 private:
  std::queue<base::OnceClosure> pending_install_requests_;
  bool should_save_requests_ = false;
};

class TestExternallyManagedAppManager : public ExternallyManagedAppManager {
 public:
  struct TestTaskResult {
    webapps::InstallResultCode code;
    bool did_install_placeholder;
  };

  TestExternallyManagedAppManager(
      Profile* profile,
      FakeWebAppProvider& provider,
      TestExternallyManagedAppInstallTaskManager& test_install_task_manager)
      : ExternallyManagedAppManager(profile),
        provider_(provider),
        test_install_task_manager_(test_install_task_manager) {}

  ~TestExternallyManagedAppManager() override {
    DCHECK(next_installation_task_results_.empty());
    DCHECK(next_installation_launch_urls_.empty());
    DCHECK(!preempt_registration_callback_);
  }

  size_t install_run_count() const { return install_run_count_; }

  const std::vector<ExternalInstallOptions>& install_options_list() {
    return install_options_list_;
  }

  size_t registration_run_count() const { return registration_run_count_; }

  const GURL& last_registered_install_url() {
    return last_registered_install_url_;
  }

  void SetNextInstallationTaskResult(const GURL& app_url,
                                     webapps::InstallResultCode result_code,
                                     bool did_install_placeholder = false) {
    DCHECK(!base::Contains(next_installation_task_results_, app_url));
    next_installation_task_results_[app_url] = {result_code,
                                                did_install_placeholder};
  }

  void SetNextInstallationLaunchURL(const GURL& app_url) {
    DCHECK(!base::Contains(next_installation_launch_urls_, app_url));
    next_installation_launch_urls_[app_url] = app_url.Resolve("launch");
  }

  bool MaybePreemptRegistration() {
    if (!preempt_registration_callback_)
      return false;

    std::optional<base::OnceClosure> callback;
    preempt_registration_callback_.swap(callback);
    std::move(*callback).Run();
    return true;
  }

  std::unique_ptr<ExternallyManagedAppInstallTask> CreateInstallationTask(
      ExternalInstallOptions install_options) override {
    return std::make_unique<TestExternallyManagedAppInstallTask>(
        this, profile(), provider(), *test_install_task_manager_,
        std::move(install_options));
  }

  std::unique_ptr<ExternallyManagedAppRegistrationTaskBase> CreateRegistration(
      GURL install_url,
      const base::TimeDelta registration_timeout) override {
    ++registration_run_count_;
    last_registered_install_url_ = install_url;
    return std::make_unique<TestExternallyManagedAppRegistrationTask>(
        install_url, this);
  }

  void OnInstallCalled(const ExternalInstallOptions& install_options) {
    ++install_run_count_;
    install_options_list_.push_back(install_options);
  }

  TestTaskResult GetNextInstallationTaskResult(const GURL& url) {
    DCHECK(base::Contains(next_installation_task_results_, url));
    auto result = next_installation_task_results_.at(url);
    next_installation_task_results_.erase(url);
    return result;
  }

  GURL GetNextInstallationLaunchURL(const GURL& url) {
    if (!base::Contains(next_installation_launch_urls_, url)) {
      return GURL();
    }

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
    ExternallyManagedAppManager::ReleaseWebContents();

    // May be called more than once, if ExternallyManagedAppManager finishes all
    // tasks before it is shutdown.
    if (!web_contents_released_event_.is_signaled()) {
      web_contents_released_event_.Signal();
    }
  }

  // Wait for ExternallyManagedAppManager to finish all installations and
  // registrations.
  void WaitForWebContentsReleased() {
    base::RunLoop run_loop;
    web_contents_released_event_.Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  FakeWebAppProvider& provider() { return *provider_; }

 private:
  class TestExternallyManagedAppInstallTask
      : public ExternallyManagedAppInstallTask {
   public:
    TestExternallyManagedAppInstallTask(
        TestExternallyManagedAppManager* externally_managed_app_manager_impl,
        Profile* profile,
        WebAppProvider& provider,
        TestExternallyManagedAppInstallTaskManager& test_install_task_manager,
        ExternalInstallOptions install_options)
        : ExternallyManagedAppInstallTask(provider, std::move(install_options)),
          externally_managed_app_manager_impl_(
              externally_managed_app_manager_impl),
          test_install_task_manager_(test_install_task_manager),
          profile_(profile),
          provider_(provider) {}

    TestExternallyManagedAppInstallTask(
        const TestExternallyManagedAppInstallTask&) = delete;
    TestExternallyManagedAppInstallTask& operator=(
        const TestExternallyManagedAppInstallTask&) = delete;
    ~TestExternallyManagedAppInstallTask() override = default;

    void DoInstall(const GURL& install_url, ResultCallback callback) {
      auto result =
          externally_managed_app_manager_impl_->GetNextInstallationTaskResult(
              install_url);
      std::optional<webapps::AppId> app_id;
      if (result.code == webapps::InstallResultCode::kSuccessNewInstall) {
        app_id = GenerateAppIdFromManifestId(
            GenerateManifestIdFromStartUrlOnly(install_url));
        GURL launch_url =
            externally_managed_app_manager_impl_->GetNextInstallationLaunchURL(
                install_url);
        const auto install_source = install_options().install_source;
        if (!provider_->registrar_unsafe().IsInstalled(*app_id)) {
          auto web_app =
              test::CreateWebApp(install_url, WebAppManagement::kPolicy);
          {
            ScopedRegistryUpdate update =
                provider_->sync_bridge_unsafe().BeginUpdate();
            update->CreateApp(std::move(web_app));
          }
          test::AddInstallUrlAndPlaceholderData(
              profile_->GetPrefs(), &provider_->sync_bridge_unsafe(), *app_id,
              install_url, install_source, result.did_install_placeholder);
        }
      }
      std::move(callback).Run(
          ExternallyManagedAppManager::InstallResult(result.code, app_id));
    }

    void Install(std::optional<webapps::AppId> placeholder_app_id,
                 ResultCallback callback) override {
      externally_managed_app_manager_impl_->OnInstallCalled(install_options());

      const GURL install_url =
          install_options().only_use_app_info_factory
              ? install_options().app_info_factory.Run()->start_url()
              : install_options().install_url;
      test_install_task_manager_->RunOrSaveRequest(base::BindLambdaForTesting(
          [&, install_url, callback = std::move(callback)]() mutable {
            DoInstall(install_url, std::move(callback));
          }));
    }

   private:
    raw_ptr<TestExternallyManagedAppManager>
        externally_managed_app_manager_impl_ = nullptr;
    const raw_ref<TestExternallyManagedAppInstallTaskManager>
        test_install_task_manager_;
    raw_ptr<Profile> profile_ = nullptr;
    raw_ref<WebAppProvider> provider_;
  };

  class TestExternallyManagedAppRegistrationTask
      : public ExternallyManagedAppRegistrationTaskBase {
   public:
    TestExternallyManagedAppRegistrationTask(
        const GURL& install_url,
        TestExternallyManagedAppManager* externally_managed_app_manager_impl)
        : ExternallyManagedAppRegistrationTaskBase(install_url,
                                                   base::Seconds(40)),
          externally_managed_app_manager_impl_(
              externally_managed_app_manager_impl) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&TestExternallyManagedAppRegistrationTask::OnProgress,
                         weak_ptr_factory_.GetWeakPtr(), install_url));
    }
    TestExternallyManagedAppRegistrationTask(
        const TestExternallyManagedAppRegistrationTask&) = delete;
    TestExternallyManagedAppRegistrationTask& operator=(
        const TestExternallyManagedAppRegistrationTask&) = delete;
    ~TestExternallyManagedAppRegistrationTask() override = default;

    void Start() override {}

   private:
    void OnProgress(const GURL& install_url) {
      if (externally_managed_app_manager_impl_->MaybePreemptRegistration())
        return;
      externally_managed_app_manager_impl_->OnRegistrationFinished(
          install_url, RegistrationResultCode::kSuccess);
    }

    const raw_ptr<TestExternallyManagedAppManager>
        externally_managed_app_manager_impl_;

    base::WeakPtrFactory<TestExternallyManagedAppRegistrationTask>
        weak_ptr_factory_{this};
  };

  const raw_ref<FakeWebAppProvider> provider_;
  TestWebAppUrlLoader test_url_loader_;
  const raw_ref<TestExternallyManagedAppInstallTaskManager>
      test_install_task_manager_;

  std::vector<ExternalInstallOptions> install_options_list_;
  GURL last_registered_install_url_;
  size_t install_run_count_ = 0;
  size_t registration_run_count_ = 0;

  std::map<GURL, TestTaskResult> next_installation_task_results_;
  std::map<GURL, GURL> next_installation_launch_urls_;
  std::optional<base::OnceClosure> preempt_registration_callback_;
  base::OneShotEvent web_contents_released_event_;
};

// TODO(crbug.com/40275639): Avoid mocking the scheduler in favor of only
// mocking external dependencies.
class TestWebAppCommandScheduler : public WebAppCommandScheduler {
 public:
  TestWebAppCommandScheduler(Profile& profile,
                             WebAppRegistrarMutable& registrar)
      : WebAppCommandScheduler(profile), registrar_(registrar) {}

  void SetNextUninstallExternalWebAppResult(const GURL& install_url,
                                            webapps::UninstallResultCode code) {
    DCHECK(
        !base::Contains(next_uninstall_external_web_app_results_, install_url));

    next_uninstall_external_web_app_results_[install_url] = {
        GenerateAppIdFromManifestId(
            GenerateManifestIdFromStartUrlOnly(install_url)),
        code};
  }

  size_t uninstall_call_count() {
    return uninstall_external_web_app_urls_.size();
  }

  const std::vector<GURL>& uninstalled_app_urls() {
    return uninstall_external_web_app_urls_;
  }

  const GURL& last_uninstalled_app_url() {
    return uninstall_external_web_app_urls_.back();
  }

  // WebAppCommandScheduler:
  void RemoveInstallUrlMaybeUninstall(
      std::optional<webapps::AppId> app_id,
      WebAppManagement::Type install_source,
      const GURL& install_url,
      webapps::WebappUninstallSource uninstall_source,
      UninstallJob::Callback callback,
      const base::Location& location = FROM_HERE) override {
    uninstall_external_web_app_urls_.push_back(install_url);

    auto [preset_app_id, code] =
        next_uninstall_external_web_app_results_[install_url];
    CHECK(!app_id.has_value() || app_id == preset_app_id);
    next_uninstall_external_web_app_results_.erase(install_url);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        location,
        base::BindLambdaForTesting(
            [&, preset_app_id, code, callback = std::move(callback)]() mutable {
              if (UninstallSucceeded(code)) {
                UnregisterApp(preset_app_id);
              }
              std::move(callback).Run(code);
            }));
  }
  void RemoveInstallManagementMaybeUninstall(
      const webapps::AppId& app_id,
      WebAppManagement::Type install_source,
      webapps::WebappUninstallSource uninstall_source,
      UninstallJob::Callback callback,
      const base::Location& location = FROM_HERE) override {
    UnregisterApp(app_id);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        location, base::BindOnce(std::move(callback),
                                 webapps::UninstallResultCode::kAppRemoved));
  }

 private:
  void UnregisterApp(const webapps::AppId& app_id) {
    auto it = registrar_->registry().find(app_id);
    CHECK(it != registrar_->registry().end(), base::NotFatalUntil::M130);
    registrar_->registry().erase(it);
  }

  raw_ref<WebAppRegistrarMutable> registrar_;
  std::vector<GURL> uninstall_external_web_app_urls_;
  // Maps app URLs to the id of the app that would have been uninstalled for
  // that url and the result of trying to uninstall it.
  std::map<GURL, std::pair<webapps::AppId, webapps::UninstallResultCode>>
      next_uninstall_external_web_app_results_;
};

}  // namespace

// Why is this called ExternallyManagedAppManagerImplTest and exists along side
// ExternallyManagedAppManagerTest unit tests?
// - Because ExternallyManagedAppManager used to be split up into
//   ExternallyManagedAppManager and ExternallyManagedAppManagerImpl and each
//   had tests written for them separately.
// - These tests are too volumous and baked with their own test suite class
//   semantics that there's no easy way to merge them together.
// Let this file be legacy unit tests and prefer adding to the
// ExternallyManagedAppManagerTest unit test suite instead.
class ExternallyManagedAppManagerImplTest : public WebAppTest {
 public:
  ExternallyManagedAppManagerImplTest() = default;
  ExternallyManagedAppManagerImplTest(
      const ExternallyManagedAppManagerImplTest&) = delete;
  ExternallyManagedAppManagerImplTest& operator=(
      const ExternallyManagedAppManagerImplTest&) = delete;

  ~ExternallyManagedAppManagerImplTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    provider_ = FakeWebAppProvider::Get(profile());
    auto externally_managed_app_manager_impl =
        std::make_unique<TestExternallyManagedAppManager>(
            profile(), provider(), test_install_task_manager_);
    externally_managed_app_manager_impl_ =
        externally_managed_app_manager_impl.get();
    provider_->SetExternallyManagedAppManager(
        std::move(externally_managed_app_manager_impl));

    auto scheduler = std::make_unique<TestWebAppCommandScheduler>(
        *profile(), provider_->GetRegistrarMutable());

    scheduler_ = scheduler.get();
    provider_->SetScheduler(std::move(scheduler));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    provider_ = nullptr;
    scheduler_ = nullptr;
    externally_managed_app_manager_impl_ = nullptr;

    WebAppTest::TearDown();
  }

 protected:
  std::pair<GURL, webapps::InstallResultCode> InstallAndWait(
      ExternallyManagedAppManager* externally_managed_app_manager,
      ExternalInstallOptions install_options) {
    base::RunLoop run_loop;

    std::optional<GURL> url;
    std::optional<webapps::InstallResultCode> code;

    externally_managed_app_manager_impl().InstallNow(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& u,
                ExternallyManagedAppManager::InstallResult result) {
              url = u;
              code = result.code;
              run_loop.Quit();
            }));
    run_loop.Run();

    return {url.value(), code.value()};
  }

  InstallAppsResults InstallAppsAndWait(
      ExternallyManagedAppManager* externally_managed_app_manager,
      std::vector<ExternalInstallOptions> apps_to_install) {
    InstallAppsResults results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_install.size(), run_loop.QuitClosure());
    externally_managed_app_manager_impl().InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url,
                ExternallyManagedAppManager::InstallResult result) {
              results.emplace_back(url, result.code);
              barrier_closure.Run();
            }));
    run_loop.Run();
    return results;
  }

  std::vector<std::pair<GURL, webapps::UninstallResultCode>>
  UninstallAppsAndWait(
      ExternallyManagedAppManager* externally_managed_app_manager,
      ExternalInstallSource install_source,
      std::vector<GURL> apps_to_uninstall) {
    std::vector<std::pair<GURL, webapps::UninstallResultCode>> results;

    base::RunLoop run_loop;
    auto barrier_closure =
        base::BarrierClosure(apps_to_uninstall.size(), run_loop.QuitClosure());
    externally_managed_app_manager->UninstallApps(
        std::move(apps_to_uninstall), install_source,
        base::BindLambdaForTesting(
            [&](const GURL& url, webapps::UninstallResultCode code) {
              results.emplace_back(url, code);
              barrier_closure.Run();
            }));
    run_loop.Run();

    return results;
  }

  // ExternalInstallOptions that was used to run the last installation task.
  const ExternalInstallOptions& last_install_options() {
    DCHECK(
        !externally_managed_app_manager_impl_->install_options_list().empty());
    return externally_managed_app_manager_impl_->install_options_list().back();
  }

  void AddAppToRegistry(std::unique_ptr<WebApp> web_app) {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  // Number of times ExternallyManagedAppInstallTask::Install was called.
  // Reflects how many times we've tried to create a web app.
  size_t install_run_count() {
    return externally_managed_app_manager_impl_->install_run_count();
  }

  // Number of times ExternallyManagedAppManager::StartRegistration was
  // called. Reflects how many times we've tried to cache service worker
  // resources for a web app.
  size_t registration_run_count() {
    return externally_managed_app_manager_impl_->registration_run_count();
  }

  const GURL& last_registered_install_url() {
    return externally_managed_app_manager_impl_->last_registered_install_url();
  }

  TestExternallyManagedAppManager& externally_managed_app_manager_impl() {
    return *externally_managed_app_manager_impl_;
  }

  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }

  WebAppSyncBridge& sync_bridge() { return provider().sync_bridge_unsafe(); }

  FakeWebAppProvider& provider() { return *provider_; }

  FakeWebAppUiManager& ui_manager() {
    return static_cast<FakeWebAppUiManager&>(provider().ui_manager());
  }

  TestExternallyManagedAppInstallTaskManager& install_task_manager() {
    return test_install_task_manager_;
  }

  TestWebAppCommandScheduler& scheduler() { return *scheduler_; }

 private:
  raw_ptr<FakeWebAppProvider> provider_;
  raw_ptr<TestWebAppCommandScheduler> scheduler_;
  raw_ptr<TestExternallyManagedAppManager> externally_managed_app_manager_impl_;

  TestExternallyManagedAppInstallTaskManager test_install_task_manager_;
};

TEST_F(ExternallyManagedAppManagerImplTest, Install_Succeeds) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kFooWebAppUrl);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                    GetInstallOptions(kFooWebAppUrl));

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
  EXPECT_EQ(kFooWebAppUrl, url);

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());

  ExternalAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kFooWebAppUrl, RegistrationResultCode::kSuccess);
  EXPECT_EQ(1U, registration_run_count());
  EXPECT_EQ(kFooWebAppUrl, last_registered_install_url());
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_SerialCallsDifferentApps) {
  // Load about:blanks twice in total, once for each install.
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kFooWebAppUrl);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  {
    auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                      GetInstallOptions(kFooWebAppUrl));

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
  }

  externally_managed_app_manager_impl().WaitForRegistrationAndCancel();
  // Foo launch URL registration will be attempted again after
  // kBarWebAppUrl installs.

  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kBarWebAppUrl);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  {
    auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                      GetInstallOptions(kBarWebAppUrl));

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kBarWebAppUrl, url);

    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
  }

  ExternalAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kFooWebAppUrl, RegistrationResultCode::kSuccess);
  ExternalAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kBarWebAppUrl, RegistrationResultCode::kSuccess);
  EXPECT_EQ(3U, registration_run_count());
  EXPECT_EQ(kBarWebAppUrl, last_registered_install_url());
}

TEST_F(ExternallyManagedAppManagerImplTest,
       Install_ConcurrentCallsDifferentApps) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  base::RunLoop run_loop;
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            // Two installations tasks should have run at this point,
            // one from the last call to install (which gets higher priority),
            // and another one for this call to install.
            EXPECT_EQ(2u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());

            run_loop.Quit();
          }));
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kBarWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kBarWebAppUrl, url);

            // The last call gets higher priority so only one
            // installation task should have run at this point.
            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
          }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_PendingSuccessfulTask) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  install_task_manager().SaveInstallRequests();
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());

            foo_run_loop.Quit();
          }));

  // Make sure the installation has started and that it hasn't finished yet.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(install_task_manager().num_pending_tasks(), 1u);

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kBarWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kBarWebAppUrl, url);

            EXPECT_EQ(2u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());

            bar_run_loop.Quit();
          }));

  install_task_manager().ProcessSavedRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  install_task_manager().ProcessSavedRequests();
  bar_run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallWithWebAppInfo_Succeeds) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  base::RunLoop foo_run_loop;

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
                      last_install_options());

            foo_run_loop.Quit();
          }));
  // Make sure the installation has started.
  foo_run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest,
       InstallAppsWithWebAppInfoAndUrl_Multiple) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptionsWithWebAppInfo(kFooWebAppUrl));
  apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(
      results,
      InstallAppsResults(
          {{kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall},
           {kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(2u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest,
       InstallWithWebAppInfo_Succeeds_Twice) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  externally_managed_app_manager_impl().Install(
      GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
                      last_install_options());
            foo_run_loop.Quit();
          }));

  foo_run_loop.Run();

  externally_managed_app_manager_impl().Install(
      GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
                      result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptionsWithWebAppInfo(kFooWebAppUrl),
                      last_install_options());

            bar_run_loop.Quit();
          }));
  bar_run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_PendingFailingTask) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kWebAppDisabled);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  install_task_manager().SaveInstallRequests();
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  base::RunLoop foo_run_loop;
  base::RunLoop bar_run_loop;

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kWebAppDisabled, result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());

            foo_run_loop.Quit();
          }));
  // Make sure the installation has started and that it hasn't finished yet.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(install_task_manager().num_pending_tasks(), 1u);

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kBarWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kBarWebAppUrl, url);

            EXPECT_EQ(2u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());

            bar_run_loop.Quit();
          }));

  install_task_manager().ProcessSavedRequests();
  foo_run_loop.Run();

  // Make sure the second installation has started.
  base::RunLoop().RunUntilIdle();

  install_task_manager().ProcessSavedRequests();
  bar_run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_ReentrantCallback) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  base::RunLoop run_loop;
  auto final_callback = base::BindLambdaForTesting(
      [&](const GURL& url, ExternallyManagedAppManager::InstallResult result) {
        EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_EQ(kBarWebAppUrl, url);

        EXPECT_EQ(2u, install_run_count());
        EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
        run_loop.Quit();
      });
  auto reentrant_callback = base::BindLambdaForTesting(
      [&](const GURL& url, ExternallyManagedAppManager::InstallResult result) {
        EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_EQ(kFooWebAppUrl, url);

        EXPECT_EQ(1u, install_run_count());
        EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());

        externally_managed_app_manager_impl().InstallNow(
            GetInstallOptions(kBarWebAppUrl), final_callback);
      });

  // Call Install() with a callback that tries to install another app.
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl), reentrant_callback);
  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_SerialCallsSameApp) {
  const GURL kFooWebAppUrl("https://foo.example");
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
    auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                      GetInstallOptions(kFooWebAppUrl));

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
  }

  {
    auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                      GetInstallOptions(kFooWebAppUrl));

    EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(1u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_ConcurrentCallsSameApp) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  base::RunLoop run_loop;
  bool first_callback_ran = false;

  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
                      result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            // Only the first installation task runs
            EXPECT_EQ(1u, install_run_count());
            EXPECT_TRUE(first_callback_ran);
            run_loop.Quit();
          }));
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kFooWebAppUrl, url);

            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
            first_callback_ran = true;
          }));
  run_loop.Run();

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_AlwaysUpdate) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  auto get_force_reinstall_info = [kFooWebAppUrl]() {
    ExternalInstallOptions options(kFooWebAppUrl,
                                   mojom::UserDisplayMode::kStandalone,
                                   ExternalInstallSource::kExternalPolicy);
    options.force_reinstall = true;
    return options;
  };

  {
    auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                      get_force_reinstall_info());

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(get_force_reinstall_info(), last_install_options());
  }

  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  {
    auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                      get_force_reinstall_info());

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    // The app should be installed again because of the |force_reinstall| flag.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(get_force_reinstall_info(), last_install_options());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_InstallationFails) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kWebAppDisabled);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                    GetInstallOptions(kFooWebAppUrl));

  EXPECT_EQ(webapps::InstallResultCode::kWebAppDisabled, code);
  EXPECT_EQ(kFooWebAppUrl, url);

  EXPECT_EQ(1u, install_run_count());
}

TEST_F(ExternallyManagedAppManagerImplTest, Install_PlaceholderApp) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
      /*did_install_placeholder=*/true);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  auto [url, code] =
      InstallAndWait(&externally_managed_app_manager_impl(), install_options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
  EXPECT_EQ(kFooWebAppUrl, url);

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(install_options, last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_Succeeds) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(
      results,
      InstallAppsResults(
          {{kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kFooWebAppUrl), last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest,
       InstallApps_FailsInstallationFails) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kWebAppDisabled);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(
      results,
      InstallAppsResults(
          {{kFooWebAppUrl, webapps::InstallResultCode::kWebAppDisabled}}));

  EXPECT_EQ(1u, install_run_count());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_PlaceholderApp) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
      /*did_install_placeholder=*/true);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;
  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(install_options);

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(
      results,
      InstallAppsResults(
          {{kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(1u, install_run_count());
  EXPECT_EQ(install_options, last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_Multiple) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));
  apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

  InstallAppsResults results = InstallAppsAndWait(
      &externally_managed_app_manager_impl(), std::move(apps_to_install));

  EXPECT_EQ(
      results,
      InstallAppsResults(
          {{kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall},
           {kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall}}));

  EXPECT_EQ(2u, install_run_count());
  EXPECT_EQ(GetInstallOptions(kBarWebAppUrl), last_install_options());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_PendingInstallApps) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  base::RunLoop run_loop;
  {
    std::vector<ExternalInstallOptions> apps_to_install;
    apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));

    externally_managed_app_manager_impl().InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url,
                ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              EXPECT_EQ(kFooWebAppUrl, url);

              EXPECT_EQ(1u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kFooWebAppUrl),
                        last_install_options());
            }));
  }

  {
    std::vector<ExternalInstallOptions> apps_to_install;
    apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

    externally_managed_app_manager_impl().InstallApps(
        std::move(apps_to_install),
        base::BindLambdaForTesting(
            [&](const GURL& url,
                ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              EXPECT_EQ(kBarWebAppUrl, url);

              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kBarWebAppUrl),
                        last_install_options());

              run_loop.Quit();
            }));
  }
  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest,
       Install_PendingMultipleInstallApps) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  const GURL kQuxWebAppUrl("https://qux.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kFooWebAppUrl);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kBarWebAppUrl);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kQuxWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
      kQuxWebAppUrl);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kQuxWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));
  apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

  // Queue through InstallApps.
  int callback_calls = 0;
  externally_managed_app_manager_impl().InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            ++callback_calls;
            if (callback_calls == 1) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              EXPECT_EQ(kFooWebAppUrl, url);

              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kFooWebAppUrl),
                        last_install_options());
            } else if (callback_calls == 2) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              EXPECT_EQ(kBarWebAppUrl, url);

              EXPECT_EQ(3u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kBarWebAppUrl),
                        last_install_options());
            } else {
              NOTREACHED_IN_MIGRATION();
            }
          }));

  // Queue through Install.
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kQuxWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kQuxWebAppUrl, url);

            // The install request from Install should be processed first.
            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kQuxWebAppUrl), last_install_options());
          }));

  ExternalAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kQuxWebAppUrl, RegistrationResultCode::kSuccess);
  ExternalAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kFooWebAppUrl, RegistrationResultCode::kSuccess);
  ExternalAppRegistrationWaiter(&externally_managed_app_manager_impl())
      .AwaitNextRegistration(kBarWebAppUrl, RegistrationResultCode::kSuccess);
  EXPECT_EQ(3U, registration_run_count());
  EXPECT_EQ(kBarWebAppUrl, last_registered_install_url());
}

TEST_F(ExternallyManagedAppManagerImplTest, InstallApps_PendingInstall) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  const GURL kQuxWebAppUrl("https://qux.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kBarWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kQuxWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kQuxWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

  base::RunLoop run_loop;

  // Queue through Install.
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kQuxWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kQuxWebAppUrl, url);

            // The install request from Install should be processed first.
            EXPECT_EQ(1u, install_run_count());
            EXPECT_EQ(GetInstallOptions(kQuxWebAppUrl), last_install_options());
          }));

  // Queue through InstallApps.
  std::vector<ExternalInstallOptions> apps_to_install;
  apps_to_install.push_back(GetInstallOptions(kFooWebAppUrl));
  apps_to_install.push_back(GetInstallOptions(kBarWebAppUrl));

  int callback_calls = 0;
  externally_managed_app_manager_impl().InstallApps(
      std::move(apps_to_install),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            ++callback_calls;
            if (callback_calls == 1) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              EXPECT_EQ(kFooWebAppUrl, url);

              // The install requests from InstallApps should be processed next.
              EXPECT_EQ(2u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kFooWebAppUrl),
                        last_install_options());

              return;
            }
            if (callback_calls == 2) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              EXPECT_EQ(kBarWebAppUrl, url);

              EXPECT_EQ(3u, install_run_count());
              EXPECT_EQ(GetInstallOptions(kBarWebAppUrl),
                        last_install_options());

              run_loop.Quit();
              return;
            }
            NOTREACHED_IN_MIGRATION();
          }));
  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, AppUninstalled) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  {
    auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                      GetInstallOptions(kFooWebAppUrl));

    EXPECT_EQ(1u, install_run_count());
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
  }

  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(kFooWebAppUrl);
  if (app_id.has_value()) {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->DeleteApp(app_id.value());
  }

  // Try to install the app again.
  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);

    auto [url, code] = InstallAndWait(&externally_managed_app_manager_impl(),
                                      GetInstallOptions(kFooWebAppUrl));

    // The app was uninstalled so a new installation task should run.
    EXPECT_EQ(2u, install_run_count());
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
  }
}

TEST_F(ExternallyManagedAppManagerImplTest, UninstallApps_Succeeds) {
  const GURL kFooWebAppUrl("https://foo.example");
  auto web_app = test::CreateWebApp(kFooWebAppUrl, WebAppManagement::kPolicy);
  AddAppToRegistry(std::move(web_app));

  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  UninstallAppsResults results = UninstallAppsAndWait(
      &externally_managed_app_manager_impl(),
      ExternalInstallSource::kExternalPolicy, std::vector<GURL>{kFooWebAppUrl});

  EXPECT_EQ(results,
            UninstallAppsResults(
                {{kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved}}));

  EXPECT_EQ(1u, scheduler().uninstall_call_count());
  EXPECT_EQ(kFooWebAppUrl, scheduler().last_uninstalled_app_url());
}

TEST_F(ExternallyManagedAppManagerImplTest, UninstallApps_Fails) {
  const GURL kFooWebAppUrl("https://foo.example");
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kError);
  UninstallAppsResults results = UninstallAppsAndWait(
      &externally_managed_app_manager_impl(),
      ExternalInstallSource::kExternalPolicy, std::vector<GURL>{kFooWebAppUrl});
  EXPECT_EQ(results,
            UninstallAppsResults(
                {{kFooWebAppUrl, webapps::UninstallResultCode::kError}}));

  EXPECT_EQ(1u, scheduler().uninstall_call_count());
  EXPECT_EQ(kFooWebAppUrl, scheduler().last_uninstalled_app_url());
}

TEST_F(ExternallyManagedAppManagerImplTest, UninstallApps_Multiple) {
  const GURL kFooWebAppUrl("https://foo.example");
  const GURL kBarWebAppUrl("https://bar.example");
  auto web_app = test::CreateWebApp(kFooWebAppUrl, WebAppManagement::kPolicy);
  AddAppToRegistry(std::move(web_app));
  web_app = test::CreateWebApp(kBarWebAppUrl, WebAppManagement::kPolicy);
  AddAppToRegistry(std::move(web_app));

  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  scheduler().SetNextUninstallExternalWebAppResult(
      kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  UninstallAppsResults results =
      UninstallAppsAndWait(&externally_managed_app_manager_impl(),
                           ExternalInstallSource::kExternalPolicy,
                           std::vector<GURL>{kFooWebAppUrl, kBarWebAppUrl});
  EXPECT_EQ(results,
            UninstallAppsResults(
                {{kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved},
                 {kBarWebAppUrl, webapps::UninstallResultCode::kAppRemoved}}));

  EXPECT_EQ(2u, scheduler().uninstall_call_count());
  EXPECT_EQ(std::vector<GURL>({kFooWebAppUrl, kBarWebAppUrl}),
            scheduler().uninstalled_app_urls());
}

TEST_F(ExternallyManagedAppManagerImplTest, UninstallApps_PendingInstall) {
  const GURL kFooWebAppUrl("https://foo.example");
  externally_managed_app_manager_impl().SetNextInstallationTaskResult(
      kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);

  base::RunLoop run_loop;
  externally_managed_app_manager_impl().InstallNow(
      GetInstallOptions(kFooWebAppUrl),
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_EQ(kFooWebAppUrl, url);
            run_loop.Quit();
          }));

  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kError);
  UninstallAppsResults uninstall_results = UninstallAppsAndWait(
      &externally_managed_app_manager_impl(),
      ExternalInstallSource::kExternalPolicy, std::vector<GURL>{kFooWebAppUrl});
  scheduler().SetNextUninstallExternalWebAppResult(
      kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
  EXPECT_EQ(uninstall_results,
            UninstallAppsResults(
                {{kFooWebAppUrl, webapps::UninstallResultCode::kError}}));
  EXPECT_EQ(1u, scheduler().uninstall_call_count());

  run_loop.Run();
}

TEST_F(ExternallyManagedAppManagerImplTest, ReinstallPlaceholderApp_Success) {
  // Install a placeholder app
  const GURL kFooWebAppUrl("https://foo.example");
  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);
    scheduler().SetNextUninstallExternalWebAppResult(
        kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
    auto [url, code] =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);
    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/false);
    auto [url, code] =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest,
       ReinstallPlaceholderApp_ReinstallNotPossible) {
  // Install a placeholder app
  const GURL kFooWebAppUrl("https://foo.example");
  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);
    scheduler().SetNextUninstallExternalWebAppResult(
        kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
    auto [url, code] =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);
    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(1u, install_run_count());
  }

  // Try to reinstall placeholder
  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);

    auto [url, code] =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    // Even though the placeholder app is already install, we make a call to
    // WebAppInstallFinalizer. WebAppInstallFinalizer ensures we don't
    // unnecessarily install the placeholder app again.
    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest,
       ReinstallPlaceholderAppWhenUnused_NoOpenedWindows) {
  // Install a placeholder app
  const GURL kFooWebAppUrl("https://foo.example");
  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);
    scheduler().SetNextUninstallExternalWebAppResult(
        kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);
    auto [url, code] =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);
    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    install_options.placeholder_resolution_behavior =
        PlaceholderResolutionBehavior::kWaitForAppWindowsClosed;
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/false);
    ui_manager().SetNumWindowsForApp(
        GenerateAppIdFromManifestId(
            GenerateManifestIdFromStartUrlOnly(kFooWebAppUrl)),
        0);

    auto [url, code] =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest,
       ReinstallPlaceholderAppWhenUnused_OneWindowOpened) {
  // Install a placeholder app
  const GURL kFooWebAppUrl("https://foo.example");
  auto install_options = GetInstallOptions(kFooWebAppUrl);
  install_options.install_placeholder = true;

  {
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/true);
    auto [url, code] =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);
    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(1u, install_run_count());
  }

  // Reinstall placeholder
  {
    webapps::AppId app_id = GenerateAppIdFromManifestId(
        GenerateManifestIdFromStartUrlOnly(kFooWebAppUrl));
    install_options.placeholder_resolution_behavior =
        PlaceholderResolutionBehavior::kWaitForAppWindowsClosed;
    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        kFooWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall,
        /*did_install_placeholder=*/false);
    ui_manager().SetNumWindowsForApp(app_id, 1);

    base::MockRepeatingCallback<void(webapps::AppId)> callback;
    EXPECT_CALL(callback, Run(app_id)).WillOnce(::testing::Invoke([&]() {
      ui_manager().SetNumWindowsForApp(app_id, 0);
    }));
    ui_manager().SetOnNotifyOnAllAppWindowsClosedCallback(callback.Get());
    scheduler().SetNextUninstallExternalWebAppResult(
        kFooWebAppUrl, webapps::UninstallResultCode::kAppRemoved);

    auto [url, code] =
        InstallAndWait(&externally_managed_app_manager_impl(), install_options);

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
    EXPECT_EQ(kFooWebAppUrl, url);

    EXPECT_EQ(2u, install_run_count());
  }
}

TEST_F(ExternallyManagedAppManagerImplTest,
       DoNotRegisterServiceWorkerForLocalApps) {
  GURL local_urls[] = {GURL("chrome://sample"),
                       GURL("chrome-untrusted://sample")};

  for (const auto& install_url : local_urls) {
    size_t prev_install_run_count = install_run_count();

    externally_managed_app_manager_impl().SetNextInstallationTaskResult(
        install_url, webapps::InstallResultCode::kSuccessNewInstall);
    externally_managed_app_manager_impl().SetNextInstallationLaunchURL(
        install_url);
    ExternalInstallOptions install_option(
        install_url, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kSystemInstalled);
    const auto& url_and_result =
        InstallAndWait(&externally_managed_app_manager_impl(), install_option);
    EXPECT_EQ(install_url, url_and_result.first);
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
              url_and_result.second);

    externally_managed_app_manager_impl().WaitForWebContentsReleased();
    EXPECT_EQ(prev_install_run_count + 1, install_run_count());
    EXPECT_EQ(0u, registration_run_count());
  }
}

}  // namespace web_app
