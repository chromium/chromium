// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"

#include "base/files/file_path.h"
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using ::testing::_;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Ne;
using ::testing::Test;
using ::testing::VariantWith;

class IwaSourceTestBase : public Test {
 protected:
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kGoogleOrigin =
      url::Origin::Create(GURL("https://google.com"));

  const base::FilePath kExamplePath =
      base::FilePath(FILE_PATH_LITERAL("example-path"));
  const base::FilePath kGooglePath =
      base::FilePath(FILE_PATH_LITERAL("google-path"));
};

using IwaSourceProxyTest = IwaSourceTestBase;

TEST_F(IwaSourceProxyTest, Works) {
  IwaSourceProxy proxy{kExampleOrigin};
  EXPECT_THAT(proxy.proxy_url(), Eq(kExampleOrigin));
  EXPECT_THAT(proxy.dev_mode(), IsTrue());

  EXPECT_THAT(IwaSourceProxy{kExampleOrigin},
              Eq(IwaSourceProxy{kExampleOrigin}));
  EXPECT_THAT(IwaSourceProxy{kExampleOrigin},
              Ne(IwaSourceProxy{kGoogleOrigin}));
}

using IwaSourceBundleTest = IwaSourceTestBase;

TEST_F(IwaSourceBundleTest, Works) {
  IwaSourceBundle bundle{kExamplePath};
  EXPECT_THAT(bundle.path(), Eq(kExamplePath));

  EXPECT_THAT(IwaSourceBundle{kExamplePath}, Eq(IwaSourceBundle{kExamplePath}));
  EXPECT_THAT(IwaSourceBundle{kExamplePath}, Ne(IwaSourceBundle{kGooglePath}));
}

TEST_F(IwaSourceBundleTest, WithModeAndFileOp) {
  IwaSourceBundle bundle{kExamplePath};
  IwaSourceBundleWithModeAndFileOp bundle_with_mode_and_file_op =
      bundle.WithModeAndFileOp(IwaSourceBundleModeAndFileOp::kDevModeMove);
  EXPECT_THAT(bundle_with_mode_and_file_op,
              Eq(IwaSourceBundleWithModeAndFileOp(
                  kExamplePath, IwaSourceBundleModeAndFileOp::kDevModeMove)));
  EXPECT_THAT(bundle_with_mode_and_file_op.path(), Eq(kExamplePath));
  EXPECT_THAT(bundle_with_mode_and_file_op.mode_and_file_op(),
              Eq(IwaSourceBundleModeAndFileOp::kDevModeMove));
  EXPECT_THAT(bundle_with_mode_and_file_op.dev_mode(), IsTrue());
}

TEST_F(IwaSourceBundleTest, WithDevModeFileOp) {
  IwaSourceBundle bundle{kExamplePath};
  IwaSourceBundleDevModeWithFileOp dev_bundle_with_file_op =
      bundle.WithDevModeFileOp(IwaSourceBundleDevFileOp::kMove);
  EXPECT_THAT(dev_bundle_with_file_op,
              Eq(IwaSourceBundleDevModeWithFileOp(
                  kExamplePath, IwaSourceBundleDevFileOp::kMove)));
  EXPECT_THAT(dev_bundle_with_file_op.path(), Eq(kExamplePath));
  EXPECT_THAT(dev_bundle_with_file_op.file_op(),
              Eq(IwaSourceBundleDevFileOp::kMove));
}

TEST_F(IwaSourceBundleTest, WithProdModeFileOp) {
  IwaSourceBundle bundle{kExamplePath};
  IwaSourceBundleProdModeWithFileOp dev_bundle_with_file_op =
      bundle.WithProdModeFileOp(IwaSourceBundleProdFileOp::kCopy);
  EXPECT_THAT(dev_bundle_with_file_op,
              Eq(IwaSourceBundleProdModeWithFileOp(
                  kExamplePath, IwaSourceBundleProdFileOp::kCopy)));
  EXPECT_THAT(dev_bundle_with_file_op.path(), Eq(kExamplePath));
  EXPECT_THAT(dev_bundle_with_file_op.file_op(),
              Eq(IwaSourceBundleProdFileOp::kCopy));
}

using IwaSourceBundleWithModeTest = IwaSourceTestBase;

TEST_F(IwaSourceBundleWithModeTest, Works) {
  IwaSourceBundleWithMode bundle{kExamplePath, /*dev_mode=*/true};
  EXPECT_THAT(bundle.path(), Eq(kExamplePath));
  EXPECT_THAT(bundle.dev_mode(), IsTrue());

  EXPECT_THAT(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true),
              Eq(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)));
  EXPECT_THAT(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/false),
              Ne(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)));
  EXPECT_THAT(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true),
              Ne(IwaSourceBundleWithMode(kGooglePath, /*dev_mode=*/true)));
}

TEST_F(IwaSourceBundleWithModeTest, FromDevOrProdMode) {
  {
    IwaSourceBundleWithMode bundle{IwaSourceBundleDevMode(kExamplePath)};
    EXPECT_THAT(bundle,
                Eq(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)));
  }
  {
    IwaSourceBundleWithMode bundle{IwaSourceBundleProdMode(kExamplePath)};
    EXPECT_THAT(bundle,
                Eq(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/false)));
  }
}

TEST_F(IwaSourceBundleWithModeTest, WithFileOp) {
  {
    IwaSourceBundleWithMode bundle{kExamplePath, /*dev_mode=*/false};
    EXPECT_THAT(
        bundle.WithFileOp(IwaSourceBundleProdFileOp::kCopy,
                          IwaSourceBundleDevFileOp::kMove),
        Eq(IwaSourceBundleWithModeAndFileOp{
            kExamplePath, IwaSourceBundleModeAndFileOp::kProdModeCopy}));
  }
  {
    IwaSourceBundleWithMode bundle{kExamplePath, /*dev_mode=*/true};
    EXPECT_THAT(bundle.WithFileOp(IwaSourceBundleProdFileOp::kCopy,
                                  IwaSourceBundleDevFileOp::kMove),
                Eq(IwaSourceBundleWithModeAndFileOp{
                    kExamplePath, IwaSourceBundleModeAndFileOp::kDevModeMove}));
  }
}

using IwaSourceBundleDevModeTest = IwaSourceTestBase;

TEST_F(IwaSourceBundleDevModeTest, Works) {
  IwaSourceBundleDevMode bundle{kExamplePath};
  EXPECT_THAT(bundle.path(), Eq(kExamplePath));

  EXPECT_THAT(IwaSourceBundleDevMode(kExamplePath),
              Eq(IwaSourceBundleDevMode(kExamplePath)));
  EXPECT_THAT(IwaSourceBundleDevMode(kExamplePath),
              Ne(IwaSourceBundleDevMode(kGooglePath)));
}

TEST_F(IwaSourceBundleDevModeTest, WithFileOp) {
  IwaSourceBundleDevMode bundle{kExamplePath};
  IwaSourceBundleDevModeWithFileOp bundle_with_file_op =
      bundle.WithFileOp(IwaSourceBundleDevFileOp::kMove);
  EXPECT_THAT(bundle_with_file_op,
              Eq(IwaSourceBundleDevModeWithFileOp(
                  kExamplePath, IwaSourceBundleDevFileOp::kMove)));
  EXPECT_THAT(bundle_with_file_op.path(), Eq(kExamplePath));
  EXPECT_THAT(bundle_with_file_op.file_op(),
              Eq(IwaSourceBundleDevFileOp::kMove));
}

using IwaSourceBundleProdModeTest = IwaSourceTestBase;

TEST_F(IwaSourceBundleProdModeTest, Works) {
  IwaSourceBundleProdMode bundle{kExamplePath};
  EXPECT_THAT(bundle.path(), Eq(kExamplePath));

  EXPECT_THAT(IwaSourceBundleProdMode(kExamplePath),
              Eq(IwaSourceBundleProdMode(kExamplePath)));
  EXPECT_THAT(IwaSourceBundleProdMode(kExamplePath),
              Ne(IwaSourceBundleProdMode(kGooglePath)));
}

TEST_F(IwaSourceBundleProdModeTest, WithFileOp) {
  IwaSourceBundleProdMode bundle{kExamplePath};
  IwaSourceBundleProdModeWithFileOp bundle_with_file_op =
      bundle.WithFileOp(IwaSourceBundleProdFileOp::kMove);
  EXPECT_THAT(bundle_with_file_op,
              Eq(IwaSourceBundleProdModeWithFileOp(
                  kExamplePath, IwaSourceBundleProdFileOp::kMove)));
  EXPECT_THAT(bundle_with_file_op.path(), Eq(kExamplePath));
  EXPECT_THAT(bundle_with_file_op.file_op(),
              Eq(IwaSourceBundleProdFileOp::kMove));
}

using IwaSourceBundleWithModeAndFileOpTest = IwaSourceTestBase;

TEST_F(IwaSourceBundleWithModeAndFileOpTest, Works) {
  using ModeAndFileOp = IwaSourceBundleWithModeAndFileOp::ModeAndFileOp;
  IwaSourceBundleWithModeAndFileOp bundle{kExamplePath,
                                          ModeAndFileOp::kDevModeMove};
  EXPECT_THAT(bundle.path(), Eq(kExamplePath));
  EXPECT_THAT(bundle.dev_mode(), IsTrue());
  EXPECT_THAT(bundle.mode_and_file_op(), Eq(ModeAndFileOp::kDevModeMove));

  EXPECT_THAT(IwaSourceBundleWithModeAndFileOp(kExamplePath,
                                               ModeAndFileOp::kDevModeMove),
              Eq(IwaSourceBundleWithModeAndFileOp(
                  kExamplePath, ModeAndFileOp::kDevModeMove)));
  EXPECT_THAT(IwaSourceBundleWithModeAndFileOp(kExamplePath,
                                               ModeAndFileOp::kProdModeMove),
              Ne(IwaSourceBundleWithModeAndFileOp(
                  kExamplePath, ModeAndFileOp::kDevModeMove)));
  EXPECT_THAT(IwaSourceBundleWithModeAndFileOp(kExamplePath,
                                               ModeAndFileOp::kDevModeMove),
              Ne(IwaSourceBundleWithModeAndFileOp(
                  kGooglePath, ModeAndFileOp::kDevModeMove)));
}

TEST_F(IwaSourceBundleWithModeAndFileOpTest, FromDevOrProdModeWithFileOp) {
  using ModeAndFileOp = IwaSourceBundleWithModeAndFileOp::ModeAndFileOp;
  {
    IwaSourceBundleWithModeAndFileOp bundle{IwaSourceBundleDevModeWithFileOp(
        kExamplePath, IwaSourceBundleDevFileOp::kMove)};
    EXPECT_THAT(bundle, Eq(IwaSourceBundleWithModeAndFileOp(
                            kExamplePath, ModeAndFileOp::kDevModeMove)));
  }
  {
    IwaSourceBundleWithModeAndFileOp bundle{IwaSourceBundleProdModeWithFileOp(
        kExamplePath, IwaSourceBundleProdFileOp::kCopy)};
    EXPECT_THAT(bundle, Eq(IwaSourceBundleWithModeAndFileOp(
                            kExamplePath, ModeAndFileOp::kProdModeCopy)));
  }
}

using IwaSourceBundleDevModeWithFileOpTest = IwaSourceTestBase;

TEST_F(IwaSourceBundleDevModeWithFileOpTest, Works) {
  using FileOp = IwaSourceBundleDevModeWithFileOp::FileOp;
  IwaSourceBundleDevModeWithFileOp bundle{kExamplePath, FileOp::kCopy};
  EXPECT_THAT(bundle.path(), Eq(kExamplePath));
  EXPECT_THAT(bundle.file_op(), Eq(FileOp::kCopy));

  EXPECT_THAT(
      IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kCopy),
      Eq(IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kCopy)));
  EXPECT_THAT(
      IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kCopy),
      Ne(IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kMove)));
  EXPECT_THAT(IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kCopy),
              Ne(IwaSourceBundleDevModeWithFileOp(kGooglePath, FileOp::kCopy)));
}

using IwaSourceBundleProdModeWithFileOpTest = IwaSourceTestBase;

TEST_F(IwaSourceBundleProdModeWithFileOpTest, Works) {
  using FileOp = IwaSourceBundleProdModeWithFileOp::FileOp;
  IwaSourceBundleProdModeWithFileOp bundle{kExamplePath, FileOp::kCopy};
  EXPECT_THAT(bundle.path(), Eq(kExamplePath));
  EXPECT_THAT(bundle.file_op(), Eq(FileOp::kCopy));

  EXPECT_THAT(
      IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy),
      Eq(IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy)));
  EXPECT_THAT(
      IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy),
      Ne(IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kMove)));
  EXPECT_THAT(
      IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy),
      Ne(IwaSourceBundleProdModeWithFileOp(kGooglePath, FileOp::kCopy)));
}

using IwaSourceTest = IwaSourceTestBase;

TEST_F(IwaSourceTest, WorksWithProxy) {
  IwaSource source = IwaSourceProxy{kExampleOrigin};
  EXPECT_THAT(source.variant(),
              VariantWith<IwaSourceProxy>(Eq(IwaSourceProxy{kExampleOrigin})));

  EXPECT_THAT(IwaSource{IwaSourceProxy{kExampleOrigin}},
              Eq(IwaSourceProxy{kExampleOrigin}));
  EXPECT_THAT(IwaSource{IwaSourceProxy{kExampleOrigin}},
              Ne(IwaSourceProxy{kGoogleOrigin}));
  EXPECT_THAT(IwaSource{IwaSourceProxy{kExampleOrigin}},
              Ne(IwaSourceBundle{kExamplePath}));
}

TEST_F(IwaSourceTest, WorksWithBundle) {
  IwaSource source = IwaSourceBundle{kExamplePath};
  EXPECT_THAT(source.variant(),
              VariantWith<IwaSourceBundle>(Eq(IwaSourceBundle{kExamplePath})));

  EXPECT_THAT(IwaSource{IwaSourceBundle{kExamplePath}},
              Eq(IwaSource{IwaSourceBundle{kExamplePath}}));
  EXPECT_THAT(IwaSource{IwaSourceBundle{kExamplePath}},
              Ne(IwaSource{IwaSourceBundle{kGooglePath}}));
  EXPECT_THAT(IwaSource{IwaSourceBundle{kExamplePath}},
              Ne(IwaSource{IwaSourceProxy{kExampleOrigin}}));
}

using IwaSourceWithModeTest = IwaSourceTestBase;

TEST_F(IwaSourceWithModeTest, Works) {
  IwaSourceWithMode source{
      IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)};
  EXPECT_THAT(source.variant(),
              VariantWith<IwaSourceBundleWithMode>(Eq(
                  IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true))));
  EXPECT_THAT(source.dev_mode(), IsTrue());

  EXPECT_THAT(IwaSourceWithMode(
                  IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)),
              Eq(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)));
  EXPECT_THAT(IwaSourceWithMode(IwaSourceBundleProdMode(kExamplePath)),
              Ne(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)));
  EXPECT_THAT(IwaSourceWithMode(
                  IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)),
              Ne(IwaSourceBundleWithMode(kGooglePath, /*dev_mode=*/true)));
}

TEST_F(IwaSourceWithModeTest, FromDevOrProdMode) {
  {
    IwaSourceWithMode source{
        IwaSourceDevMode(IwaSourceBundleDevMode(kExamplePath))};
    EXPECT_THAT(source.dev_mode(), IsTrue());
    EXPECT_THAT(source,
                Eq(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)));
  }
  {
    IwaSourceWithMode source{
        IwaSourceProdMode(IwaSourceBundleProdMode(kExamplePath))};
    EXPECT_THAT(source.dev_mode(), IsFalse());
    EXPECT_THAT(source,
                Eq(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/false)));
  }
}

TEST_F(IwaSourceWithModeTest, FromStorageLocation) {
  base::FilePath profile_dir(FILE_PATH_LITERAL("/profile-directory"));
  {
    IwaSourceWithMode source = IwaSourceWithMode::FromStorageLocation(
        profile_dir, IwaStorageOwnedBundle("ascii-dir", /*dev_mode=*/false));
    EXPECT_THAT(source.dev_mode(), IsFalse());
    EXPECT_THAT(source,
                Eq(IwaSourceBundleWithMode(profile_dir.AppendASCII("iwa")
                                               .AppendASCII("ascii-dir")
                                               .AppendASCII("main.swbn"),
                                           /*dev_mode=*/false)));
  }
  {
    IwaSourceWithMode source = IwaSourceWithMode::FromStorageLocation(
        profile_dir, IwaStorageOwnedBundle("ascii-dir", /*dev_mode=*/true));
    EXPECT_THAT(source.dev_mode(), IsTrue());
    EXPECT_THAT(source,
                Eq(IwaSourceBundleWithMode(profile_dir.AppendASCII("iwa")
                                               .AppendASCII("ascii-dir")
                                               .AppendASCII("main.swbn"),
                                           /*dev_mode=*/true)));
  }
  {
    IwaSourceWithMode source = IwaSourceWithMode::FromStorageLocation(
        profile_dir, IwaStorageUnownedBundle(kExamplePath));
    EXPECT_THAT(source.dev_mode(), IsTrue());
    EXPECT_THAT(source,
                Eq(IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)));
  }
  {
    IwaSourceWithMode source = IwaSourceWithMode::FromStorageLocation(
        profile_dir, IwaStorageProxy(kExampleOrigin));
    EXPECT_THAT(source.dev_mode(), IsTrue());
    EXPECT_THAT(source, Eq(IwaSourceProxy(kExampleOrigin)));
  }
}

TEST_F(IwaSourceWithModeTest, WithFileOp) {
  {
    IwaSourceWithMode source{
        IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/false)};
    EXPECT_THAT(
        source.WithFileOp(IwaSourceBundleProdFileOp::kCopy,
                          IwaSourceBundleDevFileOp::kMove),
        Eq(IwaSourceBundleWithModeAndFileOp{
            kExamplePath, IwaSourceBundleModeAndFileOp::kProdModeCopy}));
  }
  {
    IwaSourceWithMode source{
        IwaSourceBundleWithMode(kExamplePath, /*dev_mode=*/true)};
    EXPECT_THAT(source.WithFileOp(IwaSourceBundleProdFileOp::kCopy,
                                  IwaSourceBundleDevFileOp::kMove),
                Eq(IwaSourceBundleWithModeAndFileOp{
                    kExamplePath, IwaSourceBundleModeAndFileOp::kDevModeMove}));
  }
}

using IwaSourceDevModeTest = IwaSourceTestBase;

TEST_F(IwaSourceDevModeTest, Works) {
  IwaSourceDevMode source{IwaSourceBundleDevMode(kExamplePath)};
  EXPECT_THAT(source.variant(), VariantWith<IwaSourceBundleDevMode>(
                                    Eq(IwaSourceBundleDevMode(kExamplePath))));

  EXPECT_THAT(IwaSourceDevMode(IwaSourceBundleDevMode(kExamplePath)),
              Eq(IwaSourceBundleDevMode(kExamplePath)));
  EXPECT_THAT(IwaSourceDevMode(IwaSourceProxy(kExampleOrigin)),
              Ne(IwaSourceBundleDevMode(kExamplePath)));
  EXPECT_THAT(IwaSourceDevMode(IwaSourceBundleDevMode(kExamplePath)),
              Ne(IwaSourceBundleDevMode(kGooglePath)));
}

TEST_F(IwaSourceDevModeTest, WithFileOp) {
  IwaSourceDevMode source{IwaSourceBundleDevMode(kExamplePath)};
  IwaSourceDevModeWithFileOp source_with_file_op =
      source.WithFileOp(IwaSourceBundleDevFileOp::kMove);
  EXPECT_THAT(source_with_file_op,
              Eq(IwaSourceBundleDevModeWithFileOp(
                  kExamplePath, IwaSourceBundleDevFileOp::kMove)));
}

TEST_F(IwaSourceDevModeTest, FromStorageLocation) {
  base::FilePath profile_dir(FILE_PATH_LITERAL("/profile-directory"));
  {
    base::expected<IwaSourceDevMode, absl::monostate> result =
        IwaSourceDevMode::FromStorageLocation(
            profile_dir,
            IwaStorageOwnedBundle("ascii-dir", /*dev_mode=*/false));
    EXPECT_THAT(result, ErrorIs(_));
  }
  {
    base::expected<IwaSourceDevMode, absl::monostate> result =
        IwaSourceDevMode::FromStorageLocation(
            profile_dir, IwaStorageOwnedBundle("ascii-dir", /*dev_mode=*/true));
    EXPECT_THAT(
        result,
        ValueIs(Eq(IwaSourceBundleDevMode(profile_dir.AppendASCII("iwa")
                                              .AppendASCII("ascii-dir")
                                              .AppendASCII("main.swbn")))));
  }
  {
    base::expected<IwaSourceDevMode, absl::monostate> result =
        IwaSourceDevMode::FromStorageLocation(
            profile_dir, IwaStorageUnownedBundle(kExamplePath));
    EXPECT_THAT(result, ValueIs(Eq(IwaSourceBundleDevMode(kExamplePath))));
  }
  {
    base::expected<IwaSourceDevMode, absl::monostate> result =
        IwaSourceDevMode::FromStorageLocation(profile_dir,
                                              IwaStorageProxy(kExampleOrigin));
    EXPECT_THAT(result, ValueIs(Eq(IwaSourceProxy(kExampleOrigin))));
  }
}

using IwaSourceProdModeTest = IwaSourceTestBase;

TEST_F(IwaSourceProdModeTest, Works) {
  IwaSourceProdMode source{IwaSourceBundleProdMode(kExamplePath)};
  EXPECT_THAT(source.variant(), VariantWith<IwaSourceBundleProdMode>(
                                    Eq(IwaSourceBundleProdMode(kExamplePath))));

  EXPECT_THAT(IwaSourceProdMode(IwaSourceBundleProdMode(kExamplePath)),
              Eq(IwaSourceBundleProdMode(kExamplePath)));
  EXPECT_THAT(IwaSourceProdMode(IwaSourceBundleProdMode(kExamplePath)),
              Ne(IwaSourceBundleProdMode(kGooglePath)));
}

TEST_F(IwaSourceProdModeTest, WithFileOp) {
  IwaSourceProdMode source{IwaSourceBundleProdMode(kExamplePath)};
  IwaSourceProdModeWithFileOp source_with_file_op =
      source.WithFileOp(IwaSourceBundleProdFileOp::kCopy);
  EXPECT_THAT(source_with_file_op,
              Eq(IwaSourceBundleProdModeWithFileOp(
                  kExamplePath, IwaSourceBundleProdFileOp::kCopy)));
}

TEST_F(IwaSourceProdModeTest, FromStorageLocation) {
  base::FilePath profile_dir(FILE_PATH_LITERAL("/profile-directory"));
  {
    base::expected<IwaSourceProdMode, absl::monostate> result =
        IwaSourceProdMode::FromStorageLocation(
            profile_dir,
            IwaStorageOwnedBundle("ascii-dir", /*dev_mode=*/false));
    EXPECT_THAT(
        result,
        ValueIs(Eq(IwaSourceBundleProdMode(profile_dir.AppendASCII("iwa")
                                               .AppendASCII("ascii-dir")
                                               .AppendASCII("main.swbn")))));
  }
  {
    base::expected<IwaSourceProdMode, absl::monostate> result =
        IwaSourceProdMode::FromStorageLocation(
            profile_dir, IwaStorageOwnedBundle("ascii-dir", /*dev_mode=*/true));
    EXPECT_THAT(result, ErrorIs(_));
  }
  {
    base::expected<IwaSourceProdMode, absl::monostate> result =
        IwaSourceProdMode::FromStorageLocation(
            profile_dir, IwaStorageUnownedBundle(kExamplePath));
    EXPECT_THAT(result, ErrorIs(_));
  }
  {
    base::expected<IwaSourceProdMode, absl::monostate> result =
        IwaSourceProdMode::FromStorageLocation(profile_dir,
                                               IwaStorageProxy(kExampleOrigin));
    EXPECT_THAT(result, ErrorIs(_));
  }
}

using IwaSourceWithModeAndFileOpTest = IwaSourceTestBase;

TEST_F(IwaSourceWithModeAndFileOpTest, Works) {
  using ModeAndFileOp = IwaSourceBundleModeAndFileOp;
  IwaSourceWithModeAndFileOp source{IwaSourceBundleWithModeAndFileOp(
      kExamplePath, ModeAndFileOp::kDevModeMove)};
  EXPECT_THAT(source.variant(),
              VariantWith<IwaSourceBundleWithModeAndFileOp>(
                  Eq(IwaSourceBundleWithModeAndFileOp(
                      kExamplePath, ModeAndFileOp::kDevModeMove))));
  EXPECT_THAT(source.dev_mode(), IsTrue());

  EXPECT_THAT(IwaSourceWithModeAndFileOp(IwaSourceBundleWithModeAndFileOp(
                  kExamplePath, ModeAndFileOp::kDevModeMove)),
              Eq(IwaSourceBundleWithModeAndFileOp(
                  kExamplePath, ModeAndFileOp::kDevModeMove)));
  EXPECT_THAT(IwaSourceWithModeAndFileOp(IwaSourceBundleWithModeAndFileOp(
                  kExamplePath, ModeAndFileOp::kDevModeCopy)),
              Ne(IwaSourceBundleWithModeAndFileOp(
                  kExamplePath, ModeAndFileOp::kDevModeMove)));
  EXPECT_THAT(IwaSourceWithModeAndFileOp(IwaSourceBundleWithModeAndFileOp(
                  kExamplePath, ModeAndFileOp::kDevModeMove)),
              Ne(IwaSourceBundleWithModeAndFileOp(
                  kGooglePath, ModeAndFileOp::kDevModeMove)));
}

TEST_F(IwaSourceWithModeAndFileOpTest, FromDevOrProdModeWithFileOp) {
  using ModeAndFileOp = IwaSourceBundleModeAndFileOp;
  {
    IwaSourceWithModeAndFileOp source{
        IwaSourceWithModeAndFileOp(IwaSourceBundleWithModeAndFileOp(
            kExamplePath, ModeAndFileOp::kDevModeMove))};
    EXPECT_THAT(source.dev_mode(), IsTrue());
    EXPECT_THAT(source, Eq(IwaSourceBundleWithModeAndFileOp(
                            kExamplePath, ModeAndFileOp::kDevModeMove)));
  }
  {
    IwaSourceWithModeAndFileOp source{
        IwaSourceWithModeAndFileOp(IwaSourceBundleWithModeAndFileOp(
            kExamplePath, ModeAndFileOp::kProdModeCopy))};
    EXPECT_THAT(source.dev_mode(), IsFalse());
    EXPECT_THAT(source, Eq(IwaSourceBundleWithModeAndFileOp(
                            kExamplePath, ModeAndFileOp::kProdModeCopy)));
  }
}

using IwaSourceDevModeWithFileOpTest = IwaSourceTestBase;

TEST_F(IwaSourceDevModeWithFileOpTest, Works) {
  using FileOp = IwaSourceBundleDevFileOp;
  IwaSourceDevModeWithFileOp source{
      IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kMove)};
  EXPECT_THAT(
      source.variant(),
      VariantWith<IwaSourceBundleDevModeWithFileOp>(
          Eq(IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kMove))));

  EXPECT_THAT(
      IwaSourceDevModeWithFileOp(
          IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kMove)),
      Eq(IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kMove)));
  EXPECT_THAT(
      IwaSourceDevModeWithFileOp(IwaSourceProxy(kExampleOrigin)),
      Ne(IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kMove)));
  EXPECT_THAT(IwaSourceDevModeWithFileOp(IwaSourceBundleDevModeWithFileOp(
                  kExamplePath, FileOp::kMove)),
              Ne(IwaSourceBundleDevModeWithFileOp(kGooglePath, FileOp::kMove)));
  EXPECT_THAT(
      IwaSourceDevModeWithFileOp(
          IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kMove)),
      Ne(IwaSourceBundleDevModeWithFileOp(kExamplePath, FileOp::kCopy)));
}

using IwaSourceProdModeWithFileOpTest = IwaSourceTestBase;

TEST_F(IwaSourceProdModeWithFileOpTest, Works) {
  using FileOp = IwaSourceBundleProdFileOp;
  IwaSourceProdModeWithFileOp source{
      IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy)};
  EXPECT_THAT(
      source.variant(),
      VariantWith<IwaSourceBundleProdModeWithFileOp>(
          Eq(IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy))));

  EXPECT_THAT(
      IwaSourceProdModeWithFileOp(
          IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy)),
      Eq(IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy)));
  EXPECT_THAT(
      IwaSourceProdModeWithFileOp(
          IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy)),
      Ne(IwaSourceBundleProdModeWithFileOp(kGooglePath, FileOp::kCopy)));
  EXPECT_THAT(
      IwaSourceProdModeWithFileOp(
          IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kCopy)),
      Ne(IwaSourceBundleProdModeWithFileOp(kExamplePath, FileOp::kMove)));
}

}  // namespace

}  // namespace web_app
