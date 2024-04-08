// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_MANAGER_H_
#define COMPONENTS_TPCD_METADATA_MANAGER_H_

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/tpcd/metadata/common/manager_base.h"
#include "components/tpcd/metadata/parser.h"
#include "net/base/features.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

// GrantsSyncCallback is used to update downstream isolated services with a
// fresh copy to the grants.
using GrantsSyncCallback =
    base::RepeatingCallback<void(const ContentSettingsForOneType&)>;
namespace tpcd::metadata {

// The Manager class will hold the content setting generated from any installed
// TPCD Metadata component and will make it available within the browser process
// and keep a synced copy within the network process.
//
// These content setting will be used primarily by the CookieSettings classes
// and will affect cookie access decisions.
class Manager : public common::ManagerBase, public Parser::Observer {
 public:
  Manager(Parser* parser, GrantsSyncCallback callback);
  virtual ~Manager();

  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;

  [[nodiscard]] bool IsAllowed(const GURL& url,
                               const GURL& first_party_url,
                               content_settings::SettingInfo* out_info) const;
  [[nodiscard]] ContentSettingsForOneType GetGrants() const;

 protected:
  static Manager* GetInstance(Parser* parser, GrantsSyncCallback callback);
  void SetGrants(const ContentSettingsForOneType& grants);

 private:
  friend base::NoDestructor<Manager>;

  // Parser::Observer:
  void OnMetadataReady() override;

  raw_ptr<Parser> parser_;
  GrantsSyncCallback grants_sync_callback_;
  mutable base::Lock grants_lock_;
  // grants_ holds a `content_settings::HostIndexedContentSettings` if
  // `IsHostIndexedMetadataGrantsEnabled()` returns true, otherwise, it holds a
  // `ContentSettingsForOneType`.
  common::Grants grants_ GUARDED_BY(grants_lock_);
};

}  // namespace tpcd::metadata

#endif  // COMPONENTS_TPCD_METADATA_MANAGER_H_
