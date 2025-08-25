// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUT_H_
#define COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUT_H_

#include "url/gurl.h"

namespace ntp_tiles {
struct EnterpriseShortcut {
  enum class PolicyOrigin {
    kNoPolicy = 0,
    kNtpShortcuts = 1,
  };

  GURL url;
  std::u16string title;
  PolicyOrigin policy_origin = PolicyOrigin::kNoPolicy;
  bool is_hidden_by_user = false;
  bool allow_user_edit = false;
  bool allow_user_delete = false;

  EnterpriseShortcut();
  EnterpriseShortcut(const EnterpriseShortcut&);
  ~EnterpriseShortcut();

  bool operator==(const EnterpriseShortcut&) const;
};
}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUT_H_
