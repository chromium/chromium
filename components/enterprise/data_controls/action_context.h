// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_ACTION_CONTEXT_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_ACTION_CONTEXT_H_

#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/enterprise/data_controls/component.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace data_controls {

struct ActionSource {
  GURL url;

  // null represents a source that isn't a browser tab, for example a different
  // application or the browser's omnibox.
  absl::optional<bool> incognito;
};

struct ActionDestination {
  GURL url;

  // null represents a destination that isn't a browser tab, for example a
  // different application or the browser's omnibox.
  absl::optional<bool> incognito;

#if BUILDFLAG(IS_CHROMEOS)
  Component component = Component::kUnknownComponent;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

// Generic struct that represents metadata about an action involved in Data
// Controls.
struct ActionContext {
  ActionSource source;
  ActionDestination destination;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_ACTION_CONTEXT_H_
