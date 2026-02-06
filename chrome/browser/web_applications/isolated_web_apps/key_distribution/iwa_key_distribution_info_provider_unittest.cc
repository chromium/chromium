// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"

#include <optional>
#include <variant>

#include "base/base64.h"
#include "base/containers/extend.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "chrome/browser/web_applications/isolated_web_apps/chrome_iwa_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_histograms.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/web_package/test_support/signed_web_bundles/signature_verifier_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "components/webapps/isolated_web_apps/identity/iwa_identity_validator.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using base::test::DictionaryHasValue;
using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using testing::_;
using testing::AllOf;
using testing::DoAll;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Field;
using testing::FieldsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::IsFalse;
using testing::IsNull;
using testing::IsTrue;
using testing::Optional;
using testing::Property;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::VariantWith;
using testing::WithoutArgs;

constexpr std::array<uint8_t, 4> kExpectedKey = {0x00, 0x00, 0x00, 0x00};
constexpr char kWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";

IwaKeyDistribution CreateValidData() {
  IwaKeyDistribution key_distribution;

  IwaKeyRotations key_rotations;
  IwaKeyRotations::KeyRotationInfo kr_info;
  IwaAccessControl access_control;

  kr_info.set_expected_key(base::Base64Encode(kExpectedKey));
  key_rotations.mutable_key_rotations()->emplace(kWebBundleId,
                                                 std::move(kr_info));
  *key_distribution.mutable_key_rotation_data() = std::move(key_rotations);
  key_distribution.mutable_iwa_access_control()
      ->mutable_managed_allowlist()
      ->emplace(kWebBundleId, IwaAccessControl_ManagedAllowlistItemData());

  return key_distribution;
}

}  // namespace

class IwaIwaKeyDistributionInfoProviderTest : public testing::Test {
 public:
  void TearDown() override {
    IwaKeyDistributionInfoProvider::DestroyInstanceForTesting();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(IwaIwaKeyDistributionInfoProviderTest, LoadComponent) {
  base::HistogramTester ht;

  EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("1.0.0"),
                                              CreateValidData()),
              HasValue());

  EXPECT_THAT(ht.GetAllSamples(kIwaKeyDistributionComponentUpdateSource),
              base::BucketsAre(base::Bucket(
                  /*IwaComponentUpdateSource::kDownloaded*/ 1, 1)));
}

TEST_F(IwaIwaKeyDistributionInfoProviderTest,
       LoadComponentAndThenStaleComponent) {
  base::HistogramTester ht;

  auto data = CreateValidData();
  EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("1.0.0"), data),
              HasValue());
  EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("0.9.0"), data),
              ErrorIs(Eq(IwaComponentUpdateError::kStaleVersion)));

  EXPECT_THAT(
      ht.GetAllSamples(kIwaKeyDistributionComponentUpdateSource),
      base::BucketsAre(base::Bucket(IwaComponentUpdateSource::kDownloaded, 1)));
  EXPECT_THAT(ht.GetAllSamples(kIwaKeyDistributionComponentUpdateError),
              base::BucketsAre(
                  base::Bucket(IwaComponentUpdateError::kStaleVersion, 1)));
}

TEST_F(IwaIwaKeyDistributionInfoProviderTest, LoadComponentWrongPath) {
  base::HistogramTester ht;

  EXPECT_THAT(
      test::UpdateKeyDistributionInfo(base::Version("1.0.0"), base::FilePath()),
      ErrorIs(Eq(IwaComponentUpdateError::kFileNotFound)));

  EXPECT_THAT(ht.GetAllSamples(kIwaKeyDistributionComponentUpdateError),
              base::BucketsAre(
                  base::Bucket(IwaComponentUpdateError::kFileNotFound, 1)));
}

TEST_F(IwaIwaKeyDistributionInfoProviderTest, LoadComponentFaultyData) {
  base::HistogramTester ht;

  base::ScopedTempDir component_install_dir;
  CHECK(component_install_dir.CreateUniqueTempDir());
  auto path = component_install_dir.GetPath().AppendASCII("krc");
  CHECK(base::WriteFile(path, "not_a_proto"));

  EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("1.0.0"), path),
              ErrorIs(Eq(IwaComponentUpdateError::kProtoParsingFailure)));

  EXPECT_THAT(ht.GetAllSamples(kIwaKeyDistributionComponentUpdateError),
              base::BucketsAre(base::Bucket(
                  IwaComponentUpdateError::kProtoParsingFailure, 1)));
}

namespace {

struct DebugInfoTestParam {
  const char* test_name;
  bool is_preloaded = false;
  bool in_managed_allowlist = false;
  bool in_blocklist = false;
  bool has_special_app_permissions = false;
  bool is_key_rotated = false;
};

}  // namespace

class IwaKeyDistributionInfoProviderDataAccessTest
    : public testing::TestWithParam<DebugInfoTestParam> {
 public:
  void SetUp() override {
    const auto& param = GetParam();
    ASSERT_OK_AND_ASSIGN(auto bundle_id,
                         web_package::SignedWebBundleId::Create(kWebBundleId));

    test::KeyDistributionComponentBuilder builder(base::Version("1.0.0"),
                                                  param.is_preloaded);

    if (param.in_managed_allowlist) {
      builder.AddToManagedAllowlist(bundle_id);
    }
    if (param.in_blocklist) {
      builder.AddToBlocklist(bundle_id);
    }
    if (param.has_special_app_permissions) {
      builder.AddToSpecialAppPermissions(
          bundle_id,
          test::KeyDistributionComponentBuilder::SpecialAppPermissions{
              .skip_capture_started_notification = true});
    }
    if (param.is_key_rotated) {
      builder.AddToKeyRotations(bundle_id, kExpectedKey);
    }
    std::move(builder).Build().InjectComponentDataDirectly();
  }
};

TEST_P(IwaKeyDistributionInfoProviderDataAccessTest,
       ProviderApisReturnCorrectData) {
  const auto& param = GetParam();
  const auto& kd_data_provider =
      IwaKeyDistributionInfoProvider::GetInstanceForTesting();

  // Version test
  EXPECT_THAT(kd_data_provider.GetVersion(),
              testing::Optional(base::Version("1.0.0")));

  // IsPreloaded test (param.is_preloaded)
  EXPECT_EQ(kd_data_provider.IsPreloadedForTesting(), param.is_preloaded);

  // ManagedAllowlist test (param.in_managed_allowlist)
  EXPECT_EQ(kd_data_provider.IsManagedInstallPermitted(kWebBundleId),
            param.in_managed_allowlist);
  EXPECT_EQ(kd_data_provider.IsManagedUpdatePermitted(kWebBundleId),
            param.in_managed_allowlist);

  // Blocklist test (param.in_blocklist)
  EXPECT_EQ(kd_data_provider.IsBundleBlocklisted(kWebBundleId),
            param.in_blocklist);

  // SpecialAppPermissions test (param.has_special_app_permissions)
  if (param.has_special_app_permissions) {
    EXPECT_THAT(kd_data_provider.GetSkipMultiCaptureNotificationBundleIds(),
                ElementsAre(kWebBundleId));
    EXPECT_THAT(kd_data_provider.GetSpecialAppPermissionsInfo(kWebBundleId),
                testing::Pointee(Field(
                    &IwaKeyDistributionInfoProvider::SpecialAppPermissionsInfo::
                        skip_capture_started_notification,
                    IsTrue())));
  } else {
    EXPECT_THAT(kd_data_provider.GetSkipMultiCaptureNotificationBundleIds(),
                IsEmpty());
    EXPECT_THAT(kd_data_provider.GetSpecialAppPermissionsInfo(kWebBundleId),
                IsNull());
  }

  // KeyRotations test (param.is_key_rotated)
  if (param.is_key_rotated) {
    EXPECT_THAT(
        kd_data_provider.GetKeyRotationInfo(kWebBundleId),
        testing::Pointee(
            Field(&IwaKeyDistributionInfoProvider::KeyRotationInfo::public_key,
                  ElementsAreArray(kExpectedKey))));
  } else {
    EXPECT_THAT(kd_data_provider.GetKeyRotationInfo(kWebBundleId), IsNull());
  }
}

TEST_P(IwaKeyDistributionInfoProviderDataAccessTest,
       DebugDataReflectsComponentData) {
  const auto& param = GetParam();
  const auto& kd_data_provider =
      IwaKeyDistributionInfoProvider::GetInstanceForTesting();

  // Version test
  EXPECT_THAT(kd_data_provider.AsDebugValue(),
              DictionaryHasValue("component_version", base::Value("1.0.0")));

  // IsPreloaded test (param.is_preloaded)
  if (param.is_preloaded) {
    EXPECT_THAT(kd_data_provider.AsDebugValue(),
                DictionaryHasValue("is_preloaded", base::Value(true)));
  } else {
    EXPECT_FALSE(
        kd_data_provider.AsDebugValue().GetDict().contains("is_preloaded"));
  }

  // ManagedAllowlist test (param.in_managed_allowlist)
  EXPECT_THAT(
      kd_data_provider.AsDebugValue(),
      DictionaryHasValue(
          "managed_allowlist",
          base::Value(param.in_managed_allowlist
                          ? base::ListValue().Append(base::Value(kWebBundleId))
                          : base::ListValue())));

  // Blocklist test (param.in_blocklist)
  EXPECT_THAT(
      kd_data_provider.AsDebugValue(),
      DictionaryHasValue(
          "blocklist",
          base::Value(param.in_blocklist
                          ? base::ListValue().Append(base::Value(kWebBundleId))
                          : base::ListValue())));

  // SpecialAppPermissions test (param.has_special_app_permissions)
  EXPECT_THAT(
      kd_data_provider.AsDebugValue(),
      DictionaryHasValue(
          "special_app_permissions",
          base::Value(param.has_special_app_permissions
                          ? base::DictValue().Set(
                                kWebBundleId,
                                base::DictValue().Set(
                                    "skip_capture_started_notification", true))
                          : base::DictValue())));

  // KeyRotations test (param.is_key_rotated)
  EXPECT_THAT(
      kd_data_provider.AsDebugValue(),
      DictionaryHasValue(
          "key_rotations",
          base::Value(
              param.is_key_rotated
                  ? base::DictValue().Set(
                        kWebBundleId,
                        base::Value(base::DictValue().Set(
                            "public_key",
                            base::Value(base::Base64Encode(kExpectedKey)))))
                  : base::DictValue())));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IwaKeyDistributionInfoProviderDataAccessTest,
    testing::Values(
        DebugInfoTestParam{.test_name = "IsPreloaded", .is_preloaded = true},
        DebugInfoTestParam{.test_name = "ManagedAllowlist",
                           .in_managed_allowlist = true},
        DebugInfoTestParam{.test_name = "Blocklist", .in_blocklist = true},
        DebugInfoTestParam{.test_name = "SpecialAppPermissions",
                           .has_special_app_permissions = true},
        DebugInfoTestParam{.test_name = "KeyRotations",
                           .is_key_rotated = true}),
    [](const auto& info) { return info.param.test_name; });

class SignedWebBundleSignatureVerifierWithKeyDistributionTest
    : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    IwaIdentityValidator::CreateSingleton();
    ChromeIwaClient::CreateSingleton();
  }

  base::FilePath WriteSignedWebBundleToDisk(
      base::span<const uint8_t> signed_web_bundle) {
    base::FilePath signed_web_bundle_path;
    EXPECT_TRUE(
        CreateTemporaryFileInDir(temp_dir_.GetPath(), &signed_web_bundle_path));
    EXPECT_TRUE(base::WriteFile(signed_web_bundle_path, signed_web_bundle));
    return signed_web_bundle_path;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(SignedWebBundleSignatureVerifierWithKeyDistributionTest,
       VerifySignaturesWithKeyDistribution) {
  using Error = web_package::SignedWebBundleSignatureVerifier::Error;

  auto key_pairs = web_package::test::KeyPairs{
      web_package::test::EcdsaP256KeyPair::CreateRandom(),
      web_package::test::Ed25519KeyPair::CreateRandom()};

  web_package::test::WebBundleSigner::IntegrityBlockAttributes ib_attributes(
      {.web_bundle_id = kWebBundleId});

  auto signed_web_bundle = web_package::test::WebBundleSigner::SignBundle(
      web_package::WebBundleBuilder().CreateBundle(), key_pairs, ib_attributes);
  base::FilePath signed_web_bundle_path =
      WriteSignedWebBundleToDisk(signed_web_bundle);
  auto file = base::File(signed_web_bundle_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(file.IsValid());

  auto parsed_integrity_block =
      web_package::test::ParseIntegrityBlock(signed_web_bundle);
  EXPECT_EQ(parsed_integrity_block.web_bundle_id().id(),
            ib_attributes.web_bundle_id);

  base::HistogramTester ht;

  web_package::SignedWebBundleSignatureVerifier signature_verifier;
  EXPECT_THAT(web_package::test::VerifySignatures(signature_verifier, file,
                                                  parsed_integrity_block),
              ErrorIs(FieldsAre(
                  Error::Type::kWebBundleIdError,
                  base::StringPrintf("Web Bundle ID <%s> doesn't match any "
                                     "public key in the signature list.",
                                     kWebBundleId))));

  EXPECT_THAT(
      ht.GetAllSamples(kIwaKeyRotationInfoSource),
      base::BucketsAre(base::Bucket(KeyDistributionComponentSource::kNone, 1)));

  ASSERT_OK_AND_ASSIGN(auto kSignedWebBundleId,
                       web_package::SignedWebBundleId::Create(kWebBundleId));

  auto expected_key = std::visit(
      [](const auto& key_pair) -> base::span<const uint8_t> {
        return key_pair.public_key.bytes();
      },
      key_pairs[0]);

  EXPECT_OK(test::KeyDistributionComponentBuilder(base::Version("1.0.0"))
                .AddToKeyRotations(kSignedWebBundleId, expected_key)
                .Build()
                .UploadFromComponentFolder());

  EXPECT_THAT(web_package::test::VerifySignatures(signature_verifier, file,
                                                  parsed_integrity_block),
              HasValue());

  EXPECT_THAT(
      ht.GetAllSamples(kIwaKeyRotationInfoSource),
      base::BucketsAre(
          base::Bucket(KeyDistributionComponentSource::kNone, 1),
          base::Bucket(KeyDistributionComponentSource::kDownloaded, 1)));

  auto random_key = web_package::test::Ed25519KeyPair::CreateRandom();
  EXPECT_OK(
      test::KeyDistributionComponentBuilder(base::Version("1.0.1"))
          .AddToKeyRotations(kSignedWebBundleId, random_key.public_key.bytes())
          .Build()
          .UploadFromComponentFolder());

  EXPECT_THAT(
      web_package::test::VerifySignatures(signature_verifier, file,
                                          parsed_integrity_block),
      ErrorIs(FieldsAre(Error::Type::kWebBundleIdError,
                        HasSubstr(base::StringPrintf(
                            "Rotated key for Web Bundle ID <%s> doesn't match",
                            kWebBundleId)))));

  EXPECT_THAT(
      ht.GetAllSamples(kIwaKeyRotationInfoSource),
      base::BucketsAre(
          base::Bucket(KeyDistributionComponentSource::kNone, 1),
          base::Bucket(KeyDistributionComponentSource::kDownloaded, 2)));
}

namespace {

class MockOnDemandUpdater : public component_updater::OnDemandUpdater {
 public:
  MOCK_METHOD(void,
              OnDemandUpdate,
              (const std::string&,
               component_updater::OnDemandUpdater::Priority,
               component_updater::Callback),
              (override));
};

auto IsPreloadedComponent() {
  return ValueIs(Field("IsPreloaded", &test::IwaComponentMetadata::is_preloaded,
                       IsTrue()));
}

auto IsDownloadedComponent() {
  return ValueIs(Field("IsPreloaded", &test::IwaComponentMetadata::is_preloaded,
                       IsFalse()));
}

auto HoldsPreloadedComponentData() {
  return Property("IsPreloaded",
                  &IwaKeyDistributionInfoProvider::IsPreloadedForTesting,
                  Optional(IsTrue()));
}

auto HoldsDownloadedComponentData() {
  return Property("IsPreloaded",
                  &IwaKeyDistributionInfoProvider::IsPreloadedForTesting,
                  Optional(IsFalse()));
}

}  // namespace

class IwaIwaKeyDistributionInfoProviderReadinessTest
    : public ::testing::TestWithParam<bool> {
 public:
  using Component =
      component_updater::IwaKeyDistributionComponentInstallerPolicy;
  using Priority = component_updater::OnDemandUpdater::Priority;
  using ComponentRegistration = component_updater::ComponentRegistration;

  void SetUp() override {
    auto cus = std::make_unique<
        testing::NiceMock<component_updater::MockComponentUpdateService>>();
    cus_ = cus.get();
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(std::move(cus));
  }

  void TearDown() override {
    IwaKeyDistributionInfoProvider::DestroyInstanceForTesting();
  }

 protected:
  void RegisterComponentWithExpectationAndCallOnMaybeReadyInOrder(
      auto matcher,
      IwaKeyDistributionInfoProvider& key_provider,
      base::OnceClosure task) {
    if (register_first()) {
      ASSERT_THAT(test::RegisterIwaKeyDistributionComponentAndWaitForLoad(),
                  matcher);
      key_provider.OnBestEffortRuntimeDataReady().Post(FROM_HERE,
                                                       std::move(task));
    } else {
      key_provider.OnBestEffortRuntimeDataReady().Post(FROM_HERE,
                                                       std::move(task));
      ASSERT_THAT(test::RegisterIwaKeyDistributionComponentAndWaitForLoad(),
                  matcher);
    }
  }

  bool register_first() const { return GetParam(); }

  base::test::TaskEnvironment& task_environment() { return env_; }

  void WillRegisterAndLoadComponent(bool is_preloaded) {
    EXPECT_CALL(
        component_updater(),
        RegisterComponent(Field(&ComponentRegistration::app_id,
                                Eq("iebhnlpddlcpcfpfalldikcoeakpeoah"))))
        .WillOnce(DoAll(
            [&, is_preloaded](const ComponentRegistration& component) {
              installer_ = component.installer;
              InstallComponentAsync(installer_, base::Version("1.0.0"),
                                    is_preloaded);
            },
            Return(true)));
  }

  // Note that `load_delay` is the delta between the OnDemandUpdate() call and
  // the component loading.
  void WillRequestOnDemandUpdateWithSuccess(
      base::TimeDelta load_delay = base::Seconds(0)) {
    EXPECT_CALL(component_updater(), GetOnDemandUpdater)
        .WillOnce(ReturnRef(on_demand_updater()));

    EXPECT_CALL(on_demand_updater(),
                OnDemandUpdate("iebhnlpddlcpcfpfalldikcoeakpeoah", _, _))
        .WillOnce(WithoutArgs([&, load_delay] {
          ASSERT_TRUE(installer_);
          InstallComponentAsync(installer_, base::Version("2.0.0"),
                                /*is_preloaded=*/false, load_delay);
        }));
  }

  void WillNotRequestOnDemandUpdate() {
    EXPECT_CALL(component_updater(), GetOnDemandUpdater).Times(0);

    EXPECT_CALL(on_demand_updater(),
                OnDemandUpdate("iebhnlpddlcpcfpfalldikcoeakpeoah", _, _))
        .Times(0);
  }

  void WillRequestOnDemandUpdateWithoutSuccess() {
    EXPECT_CALL(component_updater(), GetOnDemandUpdater)
        .WillOnce(ReturnRef(on_demand_updater()));

    EXPECT_CALL(on_demand_updater(),
                OnDemandUpdate("iebhnlpddlcpcfpfalldikcoeakpeoah", _, _));
  }

 private:
  base::FilePath WriteComponentData(const base::Version& version,
                                    bool is_preloaded = false) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    dir_ = std::make_unique<base::ScopedTempDir>();
    CHECK(dir_->CreateUniqueTempDir());

    auto manifest = base::DictValue()
                        .Set("manifest_version", 1)
                        .Set("name", Component::kManifestName)
                        .Set("version", version.GetString());
    if (is_preloaded) {
      manifest.Set("is_preloaded", true);
    }

    CHECK(base::WriteFile(
        dir_->GetPath().Append(FILE_PATH_LITERAL("manifest.json")),
        *base::WriteJson(manifest)));

    IwaKeyDistribution kd_proto;
    CHECK(base::WriteFile(dir_->GetPath().Append(Component::kDataFileName),
                          kd_proto.SerializeAsString()));

    return dir_->GetPath();
  }

  void InstallComponentAsync(
      scoped_refptr<update_client::CrxInstaller> installer,
      const base::Version& component_version,
      bool is_preloaded,
      base::TimeDelta delay = base::Seconds(0)) {
    base::FilePath component_dir =
        WriteComponentData(component_version, is_preloaded);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&update_client::CrxInstaller::Install, installer,
                       component_dir,
                       /*public_key=*/"", /*install_params=*/nullptr,
                       base::DoNothing(), base::DoNothing()),
        delay);
  }

  component_updater::MockComponentUpdateService& component_updater() {
    return *cus_;
  }
  MockOnDemandUpdater& on_demand_updater() { return on_demand_updater_; }

 private:
  scoped_refptr<update_client::CrxInstaller> installer_;

  base::test::TaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  raw_ptr<component_updater::MockComponentUpdateService> cus_ = nullptr;
  testing::NiceMock<MockOnDemandUpdater> on_demand_updater_;

  std::unique_ptr<base::ScopedTempDir> dir_;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  base::test::ScopedFeatureList features_{
      component_updater::kIwaKeyDistributionComponent};
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// TODO(crbug.com/393102554): Remove this after launch.
#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList features_{features::kIsolatedWebApps};
#endif  // BUILDFLAG(IS_WIN)

  base::ScopedPathOverride user_dir_override_{
      component_updater::DIR_COMPONENT_USER};
  base::ScopedPathOverride preinstalled_dir_override{
      component_updater::DIR_COMPONENT_PREINSTALLED};
  base::ScopedPathOverride preinstalled_alt_dir_override{
      component_updater::DIR_COMPONENT_PREINSTALLED_ALT};
};

TEST_P(IwaIwaKeyDistributionInfoProviderReadinessTest,
       PreloadedComponentAndOnMaybeReadyCalledUpdateSuccess) {
  if (!register_first()) {
    GTEST_SKIP() << "Disabled until IWA become available outside of "
                    "non-initial profiles";
  }

  WillRegisterAndLoadComponent(/*is_preloaded=*/true);
  WillRequestOnDemandUpdateWithSuccess();

  auto& key_provider = IwaKeyDistributionInfoProvider::GetInstanceForTesting();
  base::test::TestFuture<void> future;

  RegisterComponentWithExpectationAndCallOnMaybeReadyInOrder(
      IsPreloadedComponent(), key_provider, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_THAT(key_provider, HoldsDownloadedComponentData());
}

TEST_P(IwaIwaKeyDistributionInfoProviderReadinessTest,
       DownloadedComponentAndOnMaybeReadyCalled) {
  WillRegisterAndLoadComponent(/*is_preloaded=*/false);
  WillNotRequestOnDemandUpdate();

  auto& key_provider = IwaKeyDistributionInfoProvider::GetInstanceForTesting();
  base::test::TestFuture<void> future;

  RegisterComponentWithExpectationAndCallOnMaybeReadyInOrder(
      IsDownloadedComponent(), key_provider, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_THAT(key_provider, HoldsDownloadedComponentData());
}

TEST_P(IwaIwaKeyDistributionInfoProviderReadinessTest,
       PreloadedComponentAndOnMaybeReadyCalledUpdateFails) {
  if (!register_first()) {
    GTEST_SKIP() << "Disabled until IWA become available outside of "
                    "non-initial profiles";
  }

  WillRegisterAndLoadComponent(/*is_preloaded=*/true);
  WillRequestOnDemandUpdateWithoutSuccess();

  auto& key_provider = IwaKeyDistributionInfoProvider::GetInstanceForTesting();

  // Not using TestFuture<> here because it advances mock time while waiting,
  // and this is something we'd like to do manually.
  bool was_called = false;
  RegisterComponentWithExpectationAndCallOnMaybeReadyInOrder(
      IsPreloadedComponent(), key_provider,
      base::BindLambdaForTesting([&] { was_called = true; }));

  ASSERT_FALSE(was_called);
  // To trigger the fallback signaller.
  task_environment().FastForwardBy(base::Seconds(15));
  ASSERT_TRUE(was_called);

  ASSERT_THAT(key_provider, HoldsPreloadedComponentData());
}

TEST_P(IwaIwaKeyDistributionInfoProviderReadinessTest,
       PreloadedComponentAndOnMaybeReadyCalledUpdateDelayedSuccess) {
  if (!register_first()) {
    GTEST_SKIP() << "Disabled until IWA become available outside of "
                    "non-initial profiles";
  }

  WillRegisterAndLoadComponent(/*is_preloaded=*/true);
  WillRequestOnDemandUpdateWithSuccess(/*load_delay=*/base::Seconds(30));

  auto& key_provider = IwaKeyDistributionInfoProvider::GetInstanceForTesting();
  base::test::TestFuture<void> future;

  RegisterComponentWithExpectationAndCallOnMaybeReadyInOrder(
      IsPreloadedComponent(), key_provider, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(key_provider, HoldsPreloadedComponentData());

  task_environment().FastForwardBy(base::Seconds(30));
  EXPECT_THAT(key_provider, HoldsDownloadedComponentData());
}

INSTANTIATE_TEST_SUITE_P(/*All*/,
                         IwaIwaKeyDistributionInfoProviderReadinessTest,
                         testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "RegisterFirst"
                                             : "CallOnMaybeReadyFirst";
                         });

}  // namespace web_app
