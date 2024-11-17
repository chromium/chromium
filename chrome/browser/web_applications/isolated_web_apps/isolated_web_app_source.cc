// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"

#include <ostream>

#include "base/files/file_path.h"
#include "base/functional/overloaded.h"
#include "base/json/values_util.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

namespace web_app {

namespace {

IwaSourceBundleModeAndFileOp ToBundleModeAndFileOp(
    IwaSourceBundleDevFileOp file_op) {
  switch (file_op) {
    case IwaSourceBundleDevFileOp::kCopy:
      return IwaSourceBundleModeAndFileOp::kDevModeCopy;
    case IwaSourceBundleDevFileOp::kMove:
      return IwaSourceBundleModeAndFileOp::kDevModeMove;
  }
}

IwaSourceBundleModeAndFileOp ToBundleModeAndFileOp(
    IwaSourceBundleProdFileOp file_op) {
  switch (file_op) {
    case IwaSourceBundleProdFileOp::kCopy:
      return IwaSourceBundleModeAndFileOp::kProdModeCopy;
    case IwaSourceBundleProdFileOp::kMove:
      return IwaSourceBundleModeAndFileOp::kProdModeMove;
  }
}

}  // namespace

IwaSourceProxy::IwaSourceProxy(url::Origin proxy_url)
    : proxy_url_(std::move(proxy_url)) {}
IwaSourceProxy::~IwaSourceProxy() = default;

bool IwaSourceProxy::operator==(const IwaSourceProxy& other) const = default;

base::Value IwaSourceProxy::ToDebugValue() const {
  return base::Value(
      base::Value::Dict().Set("proxy_url", proxy_url_.GetDebugString()));
}

std::ostream& operator<<(std::ostream& os, const IwaSourceProxy& source) {
  return os << source.ToDebugValue();
}

std::ostream& operator<<(std::ostream& os,
                         IwaSourceBundleModeAndFileOp file_op_and_mode) {
  switch (file_op_and_mode) {
    case IwaSourceBundleModeAndFileOp::kDevModeCopy:
      return os << "dev_copy";
    case IwaSourceBundleModeAndFileOp::kDevModeMove:
      return os << "dev_move";
    case IwaSourceBundleModeAndFileOp::kProdModeCopy:
      return os << "prod_copy";
    case IwaSourceBundleModeAndFileOp::kProdModeMove:
      return os << "prod_move";
  }
}

std::ostream& operator<<(std::ostream& os, IwaSourceBundleDevFileOp file_op) {
  switch (file_op) {
    case IwaSourceBundleDevFileOp::kCopy:
      return os << "copy";
    case IwaSourceBundleDevFileOp::kMove:
      return os << "move";
  }
}

std::ostream& operator<<(std::ostream& os, IwaSourceBundleProdFileOp file_op) {
  switch (file_op) {
    case IwaSourceBundleProdFileOp::kCopy:
      return os << "copy";
    case IwaSourceBundleProdFileOp::kMove:
      return os << "move";
  }
}

namespace internal {

IwaSourceBundleBase::IwaSourceBundleBase(base::FilePath path)
    : path_(std::move(path)) {}
IwaSourceBundleBase::~IwaSourceBundleBase() = default;

bool IwaSourceBundleBase::operator==(const IwaSourceBundleBase&) const =
    default;

}  // namespace internal

IwaSourceBundle::IwaSourceBundle(base::FilePath path)
    : internal::IwaSourceBundleBase(std::move(path)) {}
IwaSourceBundle::~IwaSourceBundle() = default;

IwaSourceBundle::IwaSourceBundle(IwaSourceBundleWithMode other)
    : internal::IwaSourceBundleBase(std::move(other.path_)) {}
IwaSourceBundle::IwaSourceBundle(IwaSourceBundleWithModeAndFileOp other)
    : internal::IwaSourceBundleBase(std::move(other.path_)) {}

bool IwaSourceBundle::operator==(const IwaSourceBundle& other) const = default;

IwaSourceBundleWithModeAndFileOp IwaSourceBundle::WithModeAndFileOp(
    IwaSourceBundleModeAndFileOp mode_and_file_op) const {
  return IwaSourceBundleWithModeAndFileOp(path_, mode_and_file_op);
}

IwaSourceBundleDevModeWithFileOp IwaSourceBundle::WithDevModeFileOp(
    IwaSourceBundleDevFileOp file_op) const {
  return IwaSourceBundleDevModeWithFileOp(path_, file_op);
}

IwaSourceBundleProdModeWithFileOp IwaSourceBundle::WithProdModeFileOp(
    IwaSourceBundleProdFileOp file_op) const {
  return IwaSourceBundleProdModeWithFileOp(path_, file_op);
}

base::Value IwaSourceBundle::ToDebugValue() const {
  return base::Value(
      base::Value::Dict().Set("path", base::FilePathToValue(path_)));
}

std::ostream& operator<<(std::ostream& os, const IwaSourceBundle& source) {
  return os << source.ToDebugValue();
}

IwaSourceBundleWithMode::IwaSourceBundleWithMode(base::FilePath path,
                                                 bool dev_mode)
    : internal::IwaSourceBundleBase(std::move(path)), dev_mode_(dev_mode) {}
IwaSourceBundleWithMode::~IwaSourceBundleWithMode() = default;

IwaSourceBundleWithMode::IwaSourceBundleWithMode(IwaSourceBundleDevMode other)
    : IwaSourceBundleWithMode(std::move(other.path_), /*dev_mode=*/true) {}
IwaSourceBundleWithMode::IwaSourceBundleWithMode(IwaSourceBundleProdMode other)
    : IwaSourceBundleWithMode(std::move(other.path_), /*dev_mode=*/false) {}
IwaSourceBundleWithMode::IwaSourceBundleWithMode(
    IwaSourceBundleWithModeAndFileOp other)
    : IwaSourceBundleWithMode(std::move(other.path_), other.dev_mode()) {}

bool IwaSourceBundleWithMode::operator==(
    const IwaSourceBundleWithMode& other) const = default;

[[nodiscard]] IwaSourceBundleWithModeAndFileOp
IwaSourceBundleWithMode::WithFileOp(
    IwaSourceBundleProdFileOp prod_file_op,
    IwaSourceBundleDevFileOp dev_file_op) const {
  if (dev_mode_) {
    return IwaSourceBundleWithModeAndFileOp(path_,
                                            ToBundleModeAndFileOp(dev_file_op));
  } else {
    return IwaSourceBundleWithModeAndFileOp(
        path_, ToBundleModeAndFileOp(prod_file_op));
  }
}

base::Value IwaSourceBundleWithMode::ToDebugValue() const {
  return base::Value(base::Value::Dict()
                         .Set("path", base::FilePathToValue(path_))
                         .Set("dev_mode", dev_mode_));
}

std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleWithMode& source) {
  return os << source.ToDebugValue();
}

IwaSourceBundleDevMode::IwaSourceBundleDevMode(base::FilePath path)
    : internal::IwaSourceBundleBase(std::move(path)) {}
IwaSourceBundleDevMode::~IwaSourceBundleDevMode() = default;

bool IwaSourceBundleDevMode::operator==(
    const IwaSourceBundleDevMode& other) const = default;

IwaSourceBundleDevModeWithFileOp IwaSourceBundleDevMode::WithFileOp(
    IwaSourceBundleDevFileOp file_op) const {
  return IwaSourceBundleDevModeWithFileOp(path_, file_op);
}

base::Value IwaSourceBundleDevMode::ToDebugValue() const {
  return base::Value(base::Value::Dict()
                         .Set("path", base::FilePathToValue(path_))
                         .Set("dev_mode", true));
}

std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleDevMode& source) {
  return os << source.ToDebugValue();
}

IwaSourceBundleProdMode::IwaSourceBundleProdMode(base::FilePath path)
    : internal::IwaSourceBundleBase(std::move(path)) {}
IwaSourceBundleProdMode::~IwaSourceBundleProdMode() = default;

bool IwaSourceBundleProdMode::operator==(
    const IwaSourceBundleProdMode& other) const = default;

IwaSourceBundleProdModeWithFileOp IwaSourceBundleProdMode::WithFileOp(
    IwaSourceBundleProdFileOp file_op) const {
  return IwaSourceBundleProdModeWithFileOp(path_, file_op);
}

base::Value IwaSourceBundleProdMode::ToDebugValue() const {
  return base::Value(base::Value::Dict()
                         .Set("path", base::FilePathToValue(path_))
                         .Set("dev_mode", false));
}

std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleProdMode& source) {
  return os << source.ToDebugValue();
}

IwaSourceBundleWithModeAndFileOp::IwaSourceBundleWithModeAndFileOp(
    base::FilePath path,
    ModeAndFileOp mode_and_file_op)
    : internal::IwaSourceBundleBase(std::move(path)),
      mode_and_file_op_(mode_and_file_op) {}
IwaSourceBundleWithModeAndFileOp::~IwaSourceBundleWithModeAndFileOp() = default;

IwaSourceBundleWithModeAndFileOp::IwaSourceBundleWithModeAndFileOp(
    IwaSourceBundleDevModeWithFileOp other)
    : internal::IwaSourceBundleBase(std::move(other.path_)),
      mode_and_file_op_(ToBundleModeAndFileOp(other.file_op_)) {}
IwaSourceBundleWithModeAndFileOp::IwaSourceBundleWithModeAndFileOp(
    IwaSourceBundleProdModeWithFileOp other)
    : internal::IwaSourceBundleBase(std::move(other.path_)),
      mode_and_file_op_(ToBundleModeAndFileOp(other.file_op_)) {}

bool IwaSourceBundleWithModeAndFileOp::operator==(
    const IwaSourceBundleWithModeAndFileOp& other) const = default;

bool IwaSourceBundleWithModeAndFileOp::dev_mode() const {
  switch (mode_and_file_op_) {
    case ModeAndFileOp::kDevModeCopy:
      return true;
    case ModeAndFileOp::kDevModeMove:
      return true;
    case ModeAndFileOp::kProdModeCopy:
      return false;
    case ModeAndFileOp::kProdModeMove:
      return false;
  }
}

base::Value IwaSourceBundleWithModeAndFileOp::ToDebugValue() const {
  return base::Value(
      base::Value::Dict()
          .Set("path", base::FilePathToValue(path_))
          .Set("mode_and_file_op", base::ToString(mode_and_file_op_)));
}

std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleWithModeAndFileOp& source) {
  return os << source.ToDebugValue();
}

IwaSourceBundleDevModeWithFileOp::IwaSourceBundleDevModeWithFileOp(
    base::FilePath path,
    FileOp file_op)
    : IwaSourceBundleDevMode(std::move(path)), file_op_(file_op) {}
IwaSourceBundleDevModeWithFileOp::~IwaSourceBundleDevModeWithFileOp() = default;

bool IwaSourceBundleDevModeWithFileOp::operator==(
    const IwaSourceBundleDevModeWithFileOp& other) const = default;

base::Value IwaSourceBundleDevModeWithFileOp::ToDebugValue() const {
  return base::Value(base::Value::Dict()
                         .Set("path", base::FilePathToValue(path_))
                         .Set("file_op", base::ToString(file_op_)));
}

std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleDevModeWithFileOp& source) {
  return os << source.ToDebugValue();
}

IwaSourceBundleProdModeWithFileOp::IwaSourceBundleProdModeWithFileOp(
    base::FilePath path,
    FileOp file_op)
    : IwaSourceBundleProdMode(std::move(path)), file_op_(file_op) {}
IwaSourceBundleProdModeWithFileOp::~IwaSourceBundleProdModeWithFileOp() =
    default;

bool IwaSourceBundleProdModeWithFileOp::operator==(
    const IwaSourceBundleProdModeWithFileOp& other) const = default;

base::Value IwaSourceBundleProdModeWithFileOp::ToDebugValue() const {
  return base::Value(base::Value::Dict()
                         .Set("path", base::FilePathToValue(path_))
                         .Set("file_op", base::ToString(file_op_)));
}

std::ostream& operator<<(std::ostream& os,
                         const IwaSourceBundleProdModeWithFileOp& source) {
  return os << source.ToDebugValue();
}

IwaSource::IwaSource(IwaSourceWithMode other)
    : variant_(absl::visit(
          base::Overloaded{[](auto variant_value) -> IwaSource::Variant {
            return variant_value;
          }},
          std::move(other.variant_))) {}
IwaSource::IwaSource(IwaSourceWithModeAndFileOp other)
    : variant_(absl::visit(
          base::Overloaded{[](auto variant_value) -> IwaSource::Variant {
            return variant_value;
          }},
          std::move(other.variant_))) {}

IwaSource::IwaSource(const IwaSource& other) = default;
IwaSource& IwaSource::operator=(const IwaSource& other) = default;

IwaSource::~IwaSource() = default;

bool IwaSource::operator==(const IwaSource& other) const = default;

base::Value IwaSource::ToDebugValue() const {
  return absl::visit(base::Overloaded{[](const auto& source) {
                       return source.ToDebugValue();
                     }},
                     variant_);
}

std::ostream& operator<<(std::ostream& os, const IwaSource& source) {
  return os << source.ToDebugValue();
}

// static
IwaSourceWithMode IwaSourceWithMode::FromStorageLocation(
    const base::FilePath& profile_dir,
    const IsolatedWebAppStorageLocation& storage_location) {
  return absl::visit(
      base::Overloaded{
          [&](const IwaStorageOwnedBundle& bundle) -> IwaSourceWithMode {
            return IwaSourceBundleWithMode(bundle.GetPath(profile_dir),
                                           bundle.dev_mode());
          },
          [&](const IwaStorageUnownedBundle& bundle) -> IwaSourceWithMode {
            return IwaSourceBundleWithMode(bundle.path(), bundle.dev_mode());
          },
          [&](const IwaStorageProxy& proxy) -> IwaSourceWithMode {
            return IwaSourceProxy(proxy.proxy_url());
          },
      },
      storage_location.variant());
}

IwaSourceWithMode::IwaSourceWithMode(IwaSourceDevMode other)
    : IwaSourceWithMode(absl::visit(
          base::Overloaded{
              [](auto variant_value) -> IwaSourceWithMode::Variant {
                return variant_value;
              }},
          std::move(other.variant_))) {}
IwaSourceWithMode::IwaSourceWithMode(IwaSourceProdMode other)
    : IwaSourceWithMode(absl::visit(
          base::Overloaded{
              [](auto variant_value) -> IwaSourceWithMode::Variant {
                return variant_value;
              }},
          std::move(other.variant_))) {}
IwaSourceWithMode::IwaSourceWithMode(IwaSourceWithModeAndFileOp other)
    : IwaSourceWithMode(absl::visit(
          base::Overloaded{
              [](auto variant_value) -> IwaSourceWithMode::Variant {
                return variant_value;
              }},
          std::move(other.variant_))) {}

IwaSourceWithMode::IwaSourceWithMode(const IwaSourceWithMode& other) = default;
IwaSourceWithMode& IwaSourceWithMode::operator=(
    const IwaSourceWithMode& other) = default;

IwaSourceWithMode::~IwaSourceWithMode() = default;

bool IwaSourceWithMode::operator==(const IwaSourceWithMode& other) const =
    default;

[[nodiscard]] IwaSourceWithModeAndFileOp IwaSourceWithMode::WithFileOp(
    IwaSourceBundleProdFileOp prod_file_op,
    IwaSourceBundleDevFileOp dev_file_op) const {
  return absl::visit(
      base::Overloaded{
          [&](const IwaSourceBundleWithMode& source)
              -> IwaSourceWithModeAndFileOp::Variant {
            return source.WithFileOp(prod_file_op, dev_file_op);
          },
          [&](const IwaSourceProxy& source)
              -> IwaSourceWithModeAndFileOp::Variant { return source; },
      },
      variant_);
}

bool IwaSourceWithMode::dev_mode() const {
  return absl::visit(
      base::Overloaded{[](const auto& source) { return source.dev_mode(); }},
      variant_);
}

base::Value IwaSourceWithMode::ToDebugValue() const {
  return absl::visit(base::Overloaded{[](const auto& source) {
                       return source.ToDebugValue();
                     }},
                     variant_);
}

std::ostream& operator<<(std::ostream& os, const IwaSourceWithMode& source) {
  return os << source.ToDebugValue();
}

// static
base::expected<IwaSourceDevMode, absl::monostate>
IwaSourceDevMode::FromStorageLocation(
    const base::FilePath& profile_dir,
    const IsolatedWebAppStorageLocation& storage_location) {
  return absl::visit(
      base::Overloaded{
          [&](const IwaStorageOwnedBundle& bundle)
              -> base::expected<IwaSourceDevMode, absl::monostate> {
            if (!bundle.dev_mode()) {
              return base::unexpected(absl::monostate());
            }
            return IwaSourceBundleDevMode(bundle.GetPath(profile_dir));
          },
          [&](const IwaStorageUnownedBundle& bundle)
              -> base::expected<IwaSourceDevMode, absl::monostate> {
            if (!bundle.dev_mode()) {
              return base::unexpected(absl::monostate());
            }
            return IwaSourceBundleDevMode(bundle.path());
          },
          [&](const IwaStorageProxy& proxy)
              -> base::expected<IwaSourceDevMode, absl::monostate> {
            return IwaSourceProxy(proxy.proxy_url());
          },
      },
      storage_location.variant());
}

IwaSourceDevMode::IwaSourceDevMode(IwaSourceDevModeWithFileOp other)
    : IwaSourceDevMode(absl::visit(
          base::Overloaded{[](auto variant_value) -> IwaSourceDevMode::Variant {
            return variant_value;
          }},
          std::move(other.variant_))) {}

IwaSourceDevMode::IwaSourceDevMode(const IwaSourceDevMode& other) = default;
IwaSourceDevMode& IwaSourceDevMode::operator=(const IwaSourceDevMode& other) =
    default;

IwaSourceDevMode::~IwaSourceDevMode() = default;

bool IwaSourceDevMode::operator==(const IwaSourceDevMode& other) const =
    default;

IwaSourceDevModeWithFileOp IwaSourceDevMode::WithFileOp(
    IwaSourceBundleDevFileOp file_op) const {
  return absl::visit(
      base::Overloaded{
          [&](const IwaSourceBundleDevMode& source)
              -> IwaSourceDevModeWithFileOp::Variant {
            return source.WithFileOp(file_op);
          },
          [&](const IwaSourceProxy& source)
              -> IwaSourceDevModeWithFileOp::Variant { return source; },
      },
      variant_);
}

base::Value IwaSourceDevMode::ToDebugValue() const {
  return absl::visit(base::Overloaded{[](const auto& source) {
                       return source.ToDebugValue();
                     }},
                     variant_);
}

std::ostream& operator<<(std::ostream& os, const IwaSourceDevMode& source) {
  return os << source.ToDebugValue();
}

// static
base::expected<IwaSourceProdMode, absl::monostate>
IwaSourceProdMode::FromStorageLocation(
    const base::FilePath& profile_dir,
    const IsolatedWebAppStorageLocation& storage_location) {
  return absl::visit(
      base::Overloaded{
          [&](const IwaStorageOwnedBundle& bundle)
              -> base::expected<IwaSourceProdMode, absl::monostate> {
            if (bundle.dev_mode()) {
              return base::unexpected(absl::monostate());
            }
            return IwaSourceBundleProdMode(bundle.GetPath(profile_dir));
          },
          [&](const IwaStorageUnownedBundle& bundle)
              -> base::expected<IwaSourceProdMode, absl::monostate> {
            if (bundle.dev_mode()) {
              return base::unexpected(absl::monostate());
            }
            return IwaSourceBundleProdMode(bundle.path());
          },
          [&](const IwaStorageProxy& proxy)
              -> base::expected<IwaSourceProdMode, absl::monostate> {
            CHECK(proxy.dev_mode());
            return base::unexpected(absl::monostate());
          },
      },
      storage_location.variant());
}

IwaSourceProdMode::IwaSourceProdMode(IwaSourceProdModeWithFileOp other)
    : IwaSourceProdMode(absl::visit(
          base::Overloaded{
              [](auto variant_value) -> IwaSourceProdMode::Variant {
                return variant_value;
              }},
          std::move(other.variant_))) {}

IwaSourceProdMode::IwaSourceProdMode(const IwaSourceProdMode& other) = default;
IwaSourceProdMode& IwaSourceProdMode::operator=(
    const IwaSourceProdMode& other) = default;

IwaSourceProdMode::~IwaSourceProdMode() = default;

bool IwaSourceProdMode::operator==(const IwaSourceProdMode& other) const =
    default;

IwaSourceProdModeWithFileOp IwaSourceProdMode::WithFileOp(
    IwaSourceBundleProdFileOp file_op) const {
  return absl::visit(
      base::Overloaded{[&](const IwaSourceBundleProdMode& source)
                           -> IwaSourceProdModeWithFileOp::Variant {
        return source.WithFileOp(file_op);
      }},
      variant_);
}

base::Value IwaSourceProdMode::ToDebugValue() const {
  return absl::visit(base::Overloaded{[](const auto& source) {
                       return source.ToDebugValue();
                     }},
                     variant_);
}

std::ostream& operator<<(std::ostream& os, const IwaSourceProdMode& source) {
  return os << source.ToDebugValue();
}

IwaSourceWithModeAndFileOp::IwaSourceWithModeAndFileOp(
    IwaSourceDevModeWithFileOp other)
    : IwaSourceWithModeAndFileOp(absl::visit(
          base::Overloaded{
              [](auto variant_value) -> IwaSourceWithModeAndFileOp::Variant {
                return variant_value;
              }},
          std::move(other.variant_))) {}
IwaSourceWithModeAndFileOp::IwaSourceWithModeAndFileOp(
    IwaSourceProdModeWithFileOp other)
    : IwaSourceWithModeAndFileOp(absl::visit(
          base::Overloaded{
              [](auto variant_value) -> IwaSourceWithModeAndFileOp::Variant {
                return variant_value;
              }},
          std::move(other.variant_))) {}

IwaSourceWithModeAndFileOp::IwaSourceWithModeAndFileOp(
    const IwaSourceWithModeAndFileOp& other) = default;
IwaSourceWithModeAndFileOp& IwaSourceWithModeAndFileOp::operator=(
    const IwaSourceWithModeAndFileOp& other) = default;

IwaSourceWithModeAndFileOp::~IwaSourceWithModeAndFileOp() = default;

bool IwaSourceWithModeAndFileOp::operator==(
    const IwaSourceWithModeAndFileOp& other) const = default;

bool IwaSourceWithModeAndFileOp::dev_mode() const {
  return absl::visit(
      base::Overloaded{[](const auto& source) { return source.dev_mode(); }},
      variant_);
}

base::Value IwaSourceWithModeAndFileOp::ToDebugValue() const {
  return absl::visit(base::Overloaded{[](const auto& source) {
                       return source.ToDebugValue();
                     }},
                     variant_);
}

std::ostream& operator<<(std::ostream& os,
                         const IwaSourceWithModeAndFileOp& source) {
  return os << source.ToDebugValue();
}

IwaSourceDevModeWithFileOp::IwaSourceDevModeWithFileOp(
    const IwaSourceDevModeWithFileOp& other) = default;
IwaSourceDevModeWithFileOp& IwaSourceDevModeWithFileOp::operator=(
    const IwaSourceDevModeWithFileOp& other) = default;

IwaSourceDevModeWithFileOp::~IwaSourceDevModeWithFileOp() = default;

bool IwaSourceDevModeWithFileOp::operator==(
    const IwaSourceDevModeWithFileOp& other) const = default;

base::Value IwaSourceDevModeWithFileOp::ToDebugValue() const {
  return absl::visit(base::Overloaded{[](const auto& source) {
                       return source.ToDebugValue();
                     }},
                     variant_);
}

std::ostream& operator<<(std::ostream& os,
                         const IwaSourceDevModeWithFileOp& source) {
  return os << source.ToDebugValue();
}

IwaSourceProdModeWithFileOp::IwaSourceProdModeWithFileOp(
    const IwaSourceProdModeWithFileOp& other) = default;
IwaSourceProdModeWithFileOp& IwaSourceProdModeWithFileOp::operator=(
    const IwaSourceProdModeWithFileOp& other) = default;

IwaSourceProdModeWithFileOp::~IwaSourceProdModeWithFileOp() = default;

bool IwaSourceProdModeWithFileOp::operator==(
    const IwaSourceProdModeWithFileOp& other) const = default;

base::Value IwaSourceProdModeWithFileOp::ToDebugValue() const {
  return absl::visit(base::Overloaded{[](const auto& source) {
                       return source.ToDebugValue();
                     }},
                     variant_);
}

std::ostream& operator<<(std::ostream& os,
                         const IwaSourceProdModeWithFileOp& source) {
  return os << source.ToDebugValue();
}

}  // namespace web_app
