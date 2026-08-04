// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_FLAG_DESCRIPTIONS_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_FLAG_DESCRIPTIONS_H_

#include "components/enterprise/buildflags/buildflags.h"

#if BUILDFLAG(ENTERPRISE_PROXY)

namespace enterprise_net::flag_descriptions {

extern const char kEnableDynamicRouteFetchingName[];
extern const char kEnableDynamicRouteFetchingDescription[];

}  // namespace enterprise_net::flag_descriptions

#endif  // BUILDFLAG(ENTERPRISE_PROXY)

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_FLAG_DESCRIPTIONS_H_
