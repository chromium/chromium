// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CRX_UPDATE_ITEM_H_
#define COMPONENTS_UPDATE_CLIENT_CRX_UPDATE_ITEM_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/version.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace update_client {

struct CrxUpdateItem {
  CrxUpdateItem();
  CrxUpdateItem(const CrxUpdateItem& other);
  CrxUpdateItem& operator=(const CrxUpdateItem& other);
  ~CrxUpdateItem();

  ComponentState state;

  std::string id;

  // The value of this data member is provided to the |UpdateClient| by the
  // caller by responding to the |CrxDataCallback|. If the caller can't
  // provide this value, for instance, in cases where the CRX was uninstalled,
  // then the |component| member will not be present.
  std::optional<CrxComponent> component;

  // Time when an update check for this CRX has happened.
  base::TimeTicks last_check;

  base::Version next_version;
  std::string next_fp;

  // The byte counts below are valid for the current url being fetched.
  // |total_bytes| is equal to the size of the CRX file and |downloaded_bytes|
  // represents how much has been downloaded up to that point. Since the CRX
  // downloader is attempting downloading from a set of URLs, and switching from
  // URL to URL, the value of |downloaded_bytes| may not monotonically increase.
  // A value of -1 means that the byte count is unknown.
  int64_t downloaded_bytes = -1;
  int64_t total_bytes = -1;

  // A value of -1 means that the progress is unknown.
  int install_progress = -1;

  ErrorCategory error_category = ErrorCategory::kNone;
  int error_code = 0;
  int extra_code1 = 0;
  std::map<std::string, std::string> custom_updatecheck_data;

  // The value of this data member is provided to the `UpdateClient` by the
  // `CrxInstaller` instance when the install completes.
  std::optional<CrxInstaller::Result> installer_result;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CRX_UPDATE_ITEM_H_
