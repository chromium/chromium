// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_SOURCE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_SOURCE_H_

#include <iosfwd>
#include <type_traits>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

namespace web_app {

// The classes defined in this file represent the source of an Isolated Web App.
// There are two kinds of sources: Bundle-based sources and proxy-based sources.
// These classes allow code that only deals with bundles, only deals with
// proxies, only deals with dev mode apps, etc., to be written in a type-safe
// way without having to resort to `CHECK`s or error handling for unsupported
// types of sources.
//
// Note: Not all of these forward declarations are technically required, but
// they make it easier to get an overview of the available classes.

// A source representing either a bundle or a proxy.
class IwaSource;

// A source representing either a bundle or a proxy, with additional information
// about whether the source is a dev mode or prod mode source.
class IwaSourceWithMode;
class IwaSourceDevMode;
class IwaSourceProdMode;  // can never be a proxy

// A source representing either a bundle or a proxy, with additional information
// about whether the source is a dev mode or prod mode source, as well as
// information about a file operation for bundle-based sources. The available
// file operations differ depending on whether it is a dev mode or prod mode
// source.
class IwaSourceWithModeAndFileOp;
class IwaSourceDevModeWithFileOp;
class IwaSourceProdModeWithFileOp;  // can never be a proxy

// Bundle-based sources:
class IwaSourceBundle;
class IwaSourceBundleWithMode;
class IwaSourceBundleDevMode;
class IwaSourceBundleProdMode;
class IwaSourceBundleWithModeAndFileOp;
class IwaSourceBundleDevModeWithFileOp;
class IwaSourceBundleProdModeWithFileOp;

// Proxy-based sources (there is just a single class here, because proxies are
// always dev mode and do not have a file operation):
class IwaSourceProxy;

class IwaSourceProxy {
 public:
  explicit IwaSourceProxy(url::Origin proxy_url);
  ~IwaSourceProxy();

  bool operator==(const IwaSourceProxy&) const;

  const url::Origin proxy_url() const { return proxy_url_; }
  bool dev_mode() const { return true; }

  base::Value ToDebugValue() const;

 private:
  url::Origin proxy_url_;
};
std::ostream& operator<<(std::ostream& os, const IwaSourceProxy& source);

enum class IwaSourceBundleModeAndFileOp {
  kDevModeCopy,
  kDevModeMove,

  kProdModeCopy,
  kProdModeMove,
  // References are not allowed outside of dev mode.
};
std::ostream& operator<<(std::ostream& os,
                         IwaSourceBundleModeAndFileOp bundle_mode_and_file_op);

enum class IwaSourceBundleDevFileOp {
  kCopy,
  kMove,
};
std::ostream& operator<<(std::ostream& os, IwaSourceBundleDevFileOp file_op);

enum class IwaSourceBundleProdFileOp {
  kCopy,
  kMove,
  // References are not allowed outside of dev mode.
};
std::ostream& operator<<(std::ostream& os, IwaSourceBundleProdFileOp file_op);

// TODO(crbug.com/40286084): We currently copy dev bundles into the profile
// directory to avoid caching issues caused by a bundle changing outside of the
// normal update flow. This can be caused by the developer rebuilding the
// bundle for example. In the future we may want to watch referenced bundles
// and clear cache when the bundle changes to support bundle updates without
// the developer explicitly having to perform the update step in the dev UI.
inline constexpr IwaSourceBundleDevFileOp kDefaultBundleDevFileOp =
    IwaSourceBundleDevFileOp::kCopy;

namespace internal {

class IwaSourceBundleBase {
 public:
  explicit IwaSourceBundleBase(base::FilePath);
  ~IwaSourceBundleBase();

  bool operator==(const IwaSourceBundleBase&) const;

  const base::FilePath& path() const { return path_; }

 protected:
  base::FilePath path_;
};

}  // namespace internal

class IwaSourceBundle : public internal::IwaSourceBundleBase {
 public:
  explicit IwaSourceBundle(base::FilePath);
  ~IwaSourceBundle();

  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceBundle(IwaSourceBundleWithMode other);
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceBundle(IwaSourceBundleWithModeAndFileOp other);

  bool operator==(const IwaSourceBundle&) const;

  [[nodiscard]] IwaSourceBundleWithModeAndFileOp WithModeAndFileOp(
      IwaSourceBundleModeAndFileOp mode_and_file_op) const;
  [[nodiscard]] IwaSourceBundleDevModeWithFileOp WithDevModeFileOp(
      IwaSourceBundleDevFileOp file_op) const;
  [[nodiscard]] IwaSourceBundleProdModeWithFileOp WithProdModeFileOp(
      IwaSourceBundleProdFileOp file_op) const;

  base::Value ToDebugValue() const;
};
std::ostream& operator<<(std::ostream& os, const IwaSourceBundle& source);

class IwaSourceBundleWithMode : public internal::IwaSourceBundleBase {
 public:
  friend class IwaSourceBundle;

  IwaSourceBundleWithMode(base::FilePath path, bool dev_mode);
  ~IwaSourceBundleWithMode();

  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceBundleWithMode(IwaSourceBundleDevMode other);
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceBundleWithMode(IwaSourceBundleProdMode other);
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceBundleWithMode(
      IwaSourceBundleWithModeAndFileOp other);

  bool operator==(const IwaSourceBundleWithMode&) const;

  // Depending on whether `this` is a dev mode or prod mode source, returns a
  // source with either the `prod_file_op` or `dev_file_op` file operation.
  [[nodiscard]] IwaSourceBundleWithModeAndFileOp WithFileOp(
      IwaSourceBundleProdFileOp prod_file_op,
      IwaSourceBundleDevFileOp dev_file_op) const;

  bool dev_mode() const { return dev_mode_; }

  base::Value ToDebugValue() const;

 protected:
  bool dev_mode_;
};
std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleWithMode& source);

class IwaSourceBundleDevMode : public internal::IwaSourceBundleBase {
 public:
  friend class IwaSourceBundleWithMode;

  explicit IwaSourceBundleDevMode(base::FilePath path);
  ~IwaSourceBundleDevMode();

  bool operator==(const IwaSourceBundleDevMode&) const;

  [[nodiscard]] IwaSourceBundleDevModeWithFileOp WithFileOp(
      IwaSourceBundleDevFileOp) const;

  base::Value ToDebugValue() const;
};
std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleDevMode& source);

class IwaSourceBundleProdMode : public internal::IwaSourceBundleBase {
 public:
  friend class IwaSourceBundleWithMode;

  explicit IwaSourceBundleProdMode(base::FilePath path);
  ~IwaSourceBundleProdMode();

  bool operator==(const IwaSourceBundleProdMode&) const;

  [[nodiscard]] IwaSourceBundleProdModeWithFileOp WithFileOp(
      IwaSourceBundleProdFileOp) const;

  base::Value ToDebugValue() const;
};
std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleProdMode& source);

class IwaSourceBundleWithModeAndFileOp : public internal::IwaSourceBundleBase {
 public:
  friend class IwaSourceBundle;
  friend class IwaSourceBundleWithMode;

  using ModeAndFileOp = IwaSourceBundleModeAndFileOp;

  IwaSourceBundleWithModeAndFileOp(base::FilePath path,
                                   ModeAndFileOp mode_and_file_op);
  ~IwaSourceBundleWithModeAndFileOp();

  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceBundleWithModeAndFileOp(
      IwaSourceBundleDevModeWithFileOp other);
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceBundleWithModeAndFileOp(
      IwaSourceBundleProdModeWithFileOp other);

  bool operator==(const IwaSourceBundleWithModeAndFileOp&) const;

  bool dev_mode() const;
  ModeAndFileOp mode_and_file_op() const { return mode_and_file_op_; }

  base::Value ToDebugValue() const;

 protected:
  ModeAndFileOp mode_and_file_op_;
};
std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleWithModeAndFileOp& source);

class IwaSourceBundleDevModeWithFileOp : public IwaSourceBundleDevMode {
 public:
  friend class IwaSourceBundleWithModeAndFileOp;

  using FileOp = IwaSourceBundleDevFileOp;

  IwaSourceBundleDevModeWithFileOp(base::FilePath path, FileOp file_op);
  ~IwaSourceBundleDevModeWithFileOp();

  bool operator==(const IwaSourceBundleDevModeWithFileOp&) const;

  FileOp file_op() const { return file_op_; }

  base::Value ToDebugValue() const;

 protected:
  FileOp file_op_;
};
std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleDevModeWithFileOp& source);

class IwaSourceBundleProdModeWithFileOp : public IwaSourceBundleProdMode {
 public:
  friend class IwaSourceBundleWithModeAndFileOp;

  using FileOp = IwaSourceBundleProdFileOp;

  IwaSourceBundleProdModeWithFileOp(base::FilePath path, FileOp file_op);
  ~IwaSourceBundleProdModeWithFileOp();

  bool operator==(const IwaSourceBundleProdModeWithFileOp&) const;

  FileOp file_op() const { return file_op_; }

  base::Value ToDebugValue() const;

 protected:
  FileOp file_op_;
};
std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleProdModeWithFileOp& source);

class IwaSource {
 public:
  using Variant = absl::variant<IwaSourceBundle, IwaSourceProxy>;

  template <typename V>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSource(V variant_value)
    requires(std::is_constructible_v<Variant, V>)
      : variant_(std::move(variant_value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSource(IwaSourceWithMode other);
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSource(IwaSourceWithModeAndFileOp other);

  IwaSource(const IwaSource&);
  IwaSource& operator=(const IwaSource&);

  ~IwaSource();

  bool operator==(const IwaSource&) const;

  const Variant& variant() const { return variant_; }

  base::Value ToDebugValue() const;

 private:
  Variant variant_;
};
std::ostream& operator<<(std::ostream& os, const IwaSource& source);

class IwaSourceWithMode {
 public:
  friend class IwaSource;

  using Variant = absl::variant<IwaSourceBundleWithMode, IwaSourceProxy>;

  static IwaSourceWithMode FromStorageLocation(
      const base::FilePath& profile_dir,
      const IsolatedWebAppStorageLocation& storage_location);

  template <typename V>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceWithMode(V variant_value)
    requires(std::is_constructible_v<Variant, V>)
      : variant_(std::move(variant_value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceWithMode(IwaSourceDevMode other);
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceWithMode(IwaSourceProdMode other);
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceWithMode(IwaSourceWithModeAndFileOp other);

  IwaSourceWithMode(const IwaSourceWithMode& other);
  IwaSourceWithMode& operator=(const IwaSourceWithMode& other);

  ~IwaSourceWithMode();

  bool operator==(const IwaSourceWithMode&) const;

  // Depending on whether `this` is a dev mode or prod mode source, returns a
  // source with either the `prod_file_op` or `dev_file_op` file operation.
  [[nodiscard]] IwaSourceWithModeAndFileOp WithFileOp(
      IwaSourceBundleProdFileOp prod_file_op,
      IwaSourceBundleDevFileOp dev_file_op) const;

  bool dev_mode() const;
  const Variant& variant() const { return variant_; }

  base::Value ToDebugValue() const;

 private:
  Variant variant_;
};
std::ostream& operator<<(std::ostream& os, const IwaSourceWithMode& source);

class IwaSourceDevMode {
 public:
  friend class IwaSourceWithMode;

  using Variant = absl::variant<IwaSourceBundleDevMode, IwaSourceProxy>;

  // Attempt to convert the provided `storage_location` into an instance of
  // `IwaSourceDevMode`. Will fail with an unexpected if the storage location is
  // not a dev mode storage location.
  static base::expected<IwaSourceDevMode, absl::monostate> FromStorageLocation(
      const base::FilePath& profile_dir,
      const IsolatedWebAppStorageLocation& storage_location);

  template <typename V>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceDevMode(V variant_value)
    requires(std::is_constructible_v<Variant, V>)
      : variant_(std::move(variant_value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceDevMode(IwaSourceDevModeWithFileOp other);

  IwaSourceDevMode(const IwaSourceDevMode& other);
  IwaSourceDevMode& operator=(const IwaSourceDevMode& other);

  ~IwaSourceDevMode();

  bool operator==(const IwaSourceDevMode&) const;

  [[nodiscard]] IwaSourceDevModeWithFileOp WithFileOp(
      IwaSourceBundleDevFileOp file_op) const;

  const Variant& variant() const { return variant_; }

  base::Value ToDebugValue() const;

 private:
  Variant variant_;
};
std::ostream& operator<<(std::ostream& os, const IwaSourceDevMode& source);

class IwaSourceProdMode {
 public:
  friend class IwaSourceWithMode;

  // Even though there is just one type in the variant, we use a variant for
  // consistency.
  using Variant = absl::variant<IwaSourceBundleProdMode>;

  // Attempt to convert the provided `storage_location` into an instance of
  // `IwaSourceProdMode`. Will fail with an unexpected if the storage location
  // is not a prod mode storage location.
  static base::expected<IwaSourceProdMode, absl::monostate> FromStorageLocation(
      const base::FilePath& profile_dir,
      const IsolatedWebAppStorageLocation& storage_location);

  template <typename V>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceProdMode(V variant_value)
    requires(std::is_constructible_v<Variant, V>)
      : variant_(std::move(variant_value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceProdMode(IwaSourceProdModeWithFileOp other);

  IwaSourceProdMode(const IwaSourceProdMode& other);
  IwaSourceProdMode& operator=(const IwaSourceProdMode& other);

  ~IwaSourceProdMode();

  bool operator==(const IwaSourceProdMode&) const;

  [[nodiscard]] IwaSourceProdModeWithFileOp WithFileOp(
      IwaSourceBundleProdFileOp file_op) const;

  const Variant& variant() const { return variant_; }

  base::Value ToDebugValue() const;

 private:
  Variant variant_;
};
std::ostream& operator<<(std::ostream& os, const IwaSourceProdMode& source);

class IwaSourceWithModeAndFileOp {
 public:
  friend class IwaSource;
  friend class IwaSourceWithMode;

  using Variant =
      absl::variant<IwaSourceBundleWithModeAndFileOp, IwaSourceProxy>;

  template <typename V>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceWithModeAndFileOp(V variant_value)
    requires(std::is_constructible_v<Variant, V>)
      : variant_(std::move(variant_value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceWithModeAndFileOp(IwaSourceDevModeWithFileOp other);
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceWithModeAndFileOp(IwaSourceProdModeWithFileOp other);

  IwaSourceWithModeAndFileOp(const IwaSourceWithModeAndFileOp& other);
  IwaSourceWithModeAndFileOp& operator=(
      const IwaSourceWithModeAndFileOp& other);

  ~IwaSourceWithModeAndFileOp();

  bool operator==(const IwaSourceWithModeAndFileOp&) const;

  bool dev_mode() const;
  const Variant& variant() const { return variant_; }

  base::Value ToDebugValue() const;

 private:
  Variant variant_;
};
std::ostream& operator<<(std::ostream& os,
                         const IwaSourceWithModeAndFileOp& source);

class IwaSourceDevModeWithFileOp {
 public:
  friend class IwaSourceDevMode;
  friend class IwaSourceWithModeAndFileOp;

  using Variant =
      absl::variant<IwaSourceBundleDevModeWithFileOp, IwaSourceProxy>;

  template <typename V>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceDevModeWithFileOp(V variant_value)
    requires(std::is_constructible_v<Variant, V>)
      : variant_(std::move(variant_value)) {}

  IwaSourceDevModeWithFileOp(const IwaSourceDevModeWithFileOp& other);
  IwaSourceDevModeWithFileOp& operator=(
      const IwaSourceDevModeWithFileOp& other);

  ~IwaSourceDevModeWithFileOp();

  bool operator==(const IwaSourceDevModeWithFileOp&) const;

  const Variant& variant() const { return variant_; }

  base::Value ToDebugValue() const;

 private:
  Variant variant_;
};
std::ostream& operator<<(std::ostream& os,
                         const IwaSourceDevModeWithFileOp& source);

class IwaSourceProdModeWithFileOp {
 public:
  friend class IwaSourceProdMode;
  friend class IwaSourceWithModeAndFileOp;

  // Even though there is just one type in the variant, we use a variant for
  // consistency.
  using Variant = absl::variant<IwaSourceBundleProdModeWithFileOp>;

  template <typename V>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IwaSourceProdModeWithFileOp(V variant_value)
    requires(std::is_constructible_v<Variant, V>)
      : variant_(std::move(variant_value)) {}

  IwaSourceProdModeWithFileOp(const IwaSourceProdModeWithFileOp& other);
  IwaSourceProdModeWithFileOp& operator=(
      const IwaSourceProdModeWithFileOp& other);

  ~IwaSourceProdModeWithFileOp();

  bool operator==(const IwaSourceProdModeWithFileOp&) const;

  const Variant& variant() const { return variant_; }

  base::Value ToDebugValue() const;

 private:
  Variant variant_;
};
std::ostream& operator<<(std::ostream& os,
                         const IwaSourceProdModeWithFileOp& source);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_SOURCE_H_
