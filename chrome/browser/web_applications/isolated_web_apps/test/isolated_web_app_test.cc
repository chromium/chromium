// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/one_shot_event.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "chrome/common/chrome_features.h"
#include "components/nacl/common/buildflags.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "components/nacl/browser/nacl_browser.h"
#endif  // BUILDFLAG(ENABLE_NACL)

#if !BUILDFLAG(IS_CHROMEOS)
#include "content/public/common/content_features.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {
using Component = component_updater::IwaKeyDistributionComponentInstallerPolicy;
using ComponentRegistration = component_updater::ComponentRegistration;

using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr std::string_view kIwaKeyDistributionComponentId =
    "iebhnlpddlcpcfpfalldikcoeakpeoah";

std::unique_ptr<base::ScopedTempDir> CreateIwaComponentDir(
    const base::Version& version,
    const IwaKeyDistribution& component_data,
    bool is_preloaded) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto dir = std::make_unique<base::ScopedTempDir>();
  CHECK(dir->CreateUniqueTempDir());

  auto manifest = base::Value::Dict()
                      .Set("manifest_version", 1)
                      .Set("name", Component::kManifestName)
                      .Set("version", version.GetString());
  if (is_preloaded) {
    manifest.Set("is_preloaded", true);
  }

  CHECK(
      base::WriteFile(dir->GetPath().Append(FILE_PATH_LITERAL("manifest.json")),
                      *base::WriteJson(manifest)));
  CHECK(base::WriteFile(dir->GetPath().Append(Component::kDataFileName),
                        component_data.SerializeAsString()));

  return dir;
}

}  // namespace

class IsolatedWebAppTest::IwaComponentWrapper {
 public:
  IwaComponentWrapper() {
    auto cus = std::make_unique<
        testing::NiceMock<component_updater::MockComponentUpdateService>>();
    cus_ = cus.get();
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(std::move(cus));

    ON_CALL(*cus_, GetOnDemandUpdater)
        .WillByDefault(ReturnRef(on_demand_updater()));
    ON_CALL(*cus_, RegisterComponent(Field(&ComponentRegistration::app_id,
                                           Eq(kIwaKeyDistributionComponentId))))
        .WillByDefault(DoAll(
            [&](const ComponentRegistration& component) {
              CHECK(!on_component_registered_.is_signaled())
                  << " Component registration is supposed to only happen once.";
              installer_ = component.installer;
              on_component_registered_.Signal();
            },
            Return(true)));
    component_updater::RegisterIwaKeyDistributionComponent(cus_);
  }

  void InstallComponentAsync(const base::Version& version,
                             const IwaKeyDistribution& component_data,
                             bool is_preloaded) {
    auto component_dir_path =
        WriteIwaComponentData(version, component_data, is_preloaded);
    on_component_registered_.Post(
        FROM_HERE, base::BindLambdaForTesting([&, component_dir_path] {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(&update_client::CrxInstaller::Install, installer_,
                             component_dir_path,
                             /*public_key=*/"", /*install_params=*/nullptr,
                             base::DoNothing(), base::DoNothing()));
        }));
  }

  MockOnDemandUpdater& on_demand_updater() { return on_demand_updater_; }

 private:
  base::FilePath WriteIwaComponentData(const base::Version& version,
                                       const IwaKeyDistribution& component_data,
                                       bool is_preloaded) {
    CHECK(!base::Contains(component_dirs_, version))
        << " There's already an installed component with version " << version;
    std::unique_ptr<base::ScopedTempDir> dir =
        CreateIwaComponentDir(version, component_data, is_preloaded);
    auto path = dir->GetPath();
    component_dirs_[version] = std::move(dir);
    return path;
  }

  // Owned by `g_browser_process`.
  raw_ptr<component_updater::MockComponentUpdateService> cus_ = nullptr;

  // `on_demand_updater_` is defined as StrictMock to prevent situations where
  // `OnDemandUpdate()` is dispatched to an empty implementation; this is only a
  // likely case if the initial component data is marked as preloaded by the
  // inheriting test suite.
  testing::StrictMock<MockOnDemandUpdater> on_demand_updater_;

  base::OneShotEvent on_component_registered_;
  scoped_refptr<update_client::CrxInstaller> installer_;

  base::flat_map<base::Version, std::unique_ptr<base::ScopedTempDir>>
      component_dirs_;
};

MockOnDemandUpdater::MockOnDemandUpdater() = default;
MockOnDemandUpdater::~MockOnDemandUpdater() = default;

IsolatedWebAppTest::~IsolatedWebAppTest() = default;

TestingProfile* IsolatedWebAppTest::profile() {
  return profile_.get();
}
FakeWebAppProvider& IsolatedWebAppTest::provider() {
  return *FakeWebAppProvider::Get(profile());
}
content::BrowserTaskEnvironment& IsolatedWebAppTest::task_environment() {
  return *env_;
}
network::TestURLLoaderFactory& IsolatedWebAppTest::url_loader_factory() {
  return url_loader_factory_;
}
IwaTestServerConfigurator& IsolatedWebAppTest::test_update_server() {
  return test_update_server_;
}
MockOnDemandUpdater& IsolatedWebAppTest::on_demand_updater() {
  return component_wrapper_->on_demand_updater();
}

void IsolatedWebAppTest::InstallComponentAsync(
    const base::Version& version,
    const IwaKeyDistribution& component_data) {
  component_wrapper_->InstallComponentAsync(version, component_data,
                                            /*is_preloaded=*/false);
}

void IsolatedWebAppTest::SetUp() {
  ASSERT_TRUE(profile_manager_.SetUp());
  profile_ = profile_manager_.CreateTestingProfile(
      TestingProfile::kDefaultProfileUserName, /*testing_factories=*/{},
      url_loader_factory_.GetSafeWeakWrapper());

#if BUILDFLAG(ENABLE_NACL)
  // Clearing Cache will clear PNACL cache, which needs this delegate set.
  nacl::NaClBrowser::SetDelegate(std::make_unique<NaClBrowserDelegateImpl>(
      profile_manager_.profile_manager()));
#endif  // BUILDFLAG(ENABLE_NACL)

  component_wrapper_ = std::make_unique<IwaComponentWrapper>();
  component_wrapper_->InstallComponentAsync(GetIwaComponentVersion(),
                                            GetIwaComponentData(),
                                            IsIwaComponentPreloaded());
}

void IsolatedWebAppTest::TearDown() {
  task_environment().RunUntilIdle();
  // Manually shut down the provider and subsystems so that async tasks are
  // stopped.
  // Note: `DeleteAllTestingProfiles` doesn't actually destruct profiles and
  // therefore doesn't Shutdown keyed services like the provider.
  provider().Shutdown();

  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();

  if (task_environment().UsesMockTime()) {
    // TODO(crbug.com/299074540): Without this line, subsequent tests are unable
    // to use `test::UninstallWebApp`, which will hang forever. This has
    // something to do with the combination of `MOCK_TIME` and NaCl, because
    // the code ends up hanging forever in
    // `PnaclTranslationCache::DoomEntriesBetween`. A simple `FastForwardBy`
    // here seems to alleviate this issue.
    task_environment().FastForwardBy(TestTimeouts::tiny_timeout());
  }

#if BUILDFLAG(ENABLE_NACL)
  nacl::NaClBrowser::ClearAndDeleteDelegate();
#endif  // BUILDFLAG(ENABLE_NACL)

  component_wrapper_.reset();
  os_integration_test_override_.reset();

  profile_ = nullptr;
  profile_manager_.DeleteAllTestingProfiles();

  IwaKeyDistributionInfoProvider::GetInstance()->DestroyInstanceForTesting();

  env_.reset();
}

IsolatedWebAppTest::IsolatedWebAppTest(
    std::unique_ptr<content::BrowserTaskEnvironment> env,
    bool dev_mode)
    : env_(std::move(env)) {
  std::vector<base::test::FeatureRef> enabled_features = {
#if !BUILDFLAG(IS_CHROMEOS)
      features::kIsolatedWebApps,
#endif  // !BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      component_updater::kIwaKeyDistributionComponent
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  };
  if (dev_mode) {
    enabled_features.push_back(features::kIsolatedWebAppDevMode);
  }
  features_.InitWithFeatures(enabled_features, {});
}

base::Version IsolatedWebAppTest::GetIwaComponentVersion() const {
  return base::Version("1.0.0");
}

IwaKeyDistribution IsolatedWebAppTest::GetIwaComponentData() const {
  return IwaKeyDistribution();
}

bool IsolatedWebAppTest::IsIwaComponentPreloaded() const {
  return false;
}

}  // namespace web_app
