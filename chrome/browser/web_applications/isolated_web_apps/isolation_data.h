// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATION_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATION_DATA_H_

#include <optional>
#include <set>
#include <string>

#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"

namespace web_app {

// Represents IWA-specific pieces of a Web App.
class IsolationData {
 public:
  struct PendingUpdateInfo {
    PendingUpdateInfo(IsolatedWebAppStorageLocation location,
                      base::Version version,
                      std::optional<IsolatedWebAppIntegrityBlockData>
                          integrity_block_data = std::nullopt);
    ~PendingUpdateInfo();
    PendingUpdateInfo(const PendingUpdateInfo&);
    PendingUpdateInfo& operator=(const PendingUpdateInfo&);

    bool operator==(const PendingUpdateInfo&) const = default;

    base::Value AsDebugValue() const;
    friend std::ostream& operator<<(std::ostream& os,
                                    const PendingUpdateInfo& update_info) {
      return os << update_info.AsDebugValue();
    }

    IsolatedWebAppStorageLocation location;
    base::Version version;

    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data;
  };

  ~IsolationData();
  IsolationData(const IsolationData&);
  IsolationData& operator=(const IsolationData&);
  IsolationData(IsolationData&&);
  IsolationData& operator=(IsolationData&&);

  bool operator==(const IsolationData&) const = default;

  base::Value AsDebugValue() const;
  friend std::ostream& operator<<(std::ostream& os,
                                  const IsolationData& isolation_data) {
    return os << isolation_data.AsDebugValue();
  }

  const IsolatedWebAppStorageLocation& location() const { return location_; }
  const base::Version& version() const { return version_; }
  const std::set<std::string>& controlled_frame_partitions() const {
    return controlled_frame_partitions_;
  }
  const std::optional<PendingUpdateInfo>& pending_update_info() const {
    return pending_update_info_;
  }
  const std::optional<IsolatedWebAppIntegrityBlockData>& integrity_block_data()
      const {
    return integrity_block_data_;
  }

 private:
  IsolationData(
      IsolatedWebAppStorageLocation location,
      base::Version version,
      std::set<std::string> controlled_frame_partitions,
      std::optional<PendingUpdateInfo> pending_update_info,
      std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data);

  IsolatedWebAppStorageLocation location_;
  base::Version version_;

  std::set<std::string> controlled_frame_partitions_;

  // If present, signals that an update for this app is available locally and
  // waiting to be applied.
  std::optional<PendingUpdateInfo> pending_update_info_;

  // Might be nullopt if this IWA is not backed by a signed web bundle (for
  // instance, in case of a proxy mode installation).
  // This field is used to prevent redundant update attempts in case of key
  // rotation by comparing the stored public keys against the rotated key.
  // key. Please don't rely on it for anything security-critical!
  std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data_;

 public:
  class Builder {
   public:
    Builder(IsolatedWebAppStorageLocation location, base::Version version);
    explicit Builder(const IsolationData& isolation_data);
    ~Builder();

    Builder(const Builder&);
    Builder& operator=(const Builder&);
    Builder(Builder&&);
    Builder& operator=(Builder&&);

    Builder& SetControlledFramePartitions(
        std::set<std::string> controlled_frame_partitions) &;
    Builder&& SetControlledFramePartitions(
        std::set<std::string> controlled_frame_partitions) &&;

    // Will `CHECK` if dev mode is different between
    // `pending_update_info.location` and `location`. In other words, a dev mode
    // owned bundle can never be updated to a prod mode owned bundle.
    Builder& SetPendingUpdateInfo(
        IsolationData::PendingUpdateInfo pending_update_info) &;
    Builder&& SetPendingUpdateInfo(
        IsolationData::PendingUpdateInfo pending_update_info) &&;

    Builder& ClearPendingUpdateInfo() &;
    Builder&& ClearPendingUpdateInfo() &&;

    Builder& SetIntegrityBlockData(
        IsolatedWebAppIntegrityBlockData integrity_block_data) &;
    Builder&& SetIntegrityBlockData(
        IsolatedWebAppIntegrityBlockData integrity_block_data) &&;

    IsolationData Build() &&;

   private:
    IsolatedWebAppStorageLocation location_;
    base::Version version_;

    std::set<std::string> controlled_frame_partitions_;
    std::optional<IsolationData::PendingUpdateInfo> pending_update_info_;
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data_;
  };
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATION_DATA_H_
