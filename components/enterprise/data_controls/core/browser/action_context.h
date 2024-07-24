// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_ACTION_CONTEXT_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_ACTION_CONTEXT_H_

#include "build/chromeos_buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/enterprise/data_controls/core/browser/component.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace data_controls {

struct ActionSource {
  // Returns true if all fields are set to default values.
  bool empty() const;

  GURL url;
  bool incognito = false;

  // Indicates that the source of the data is the OS clipboard. If this is
  // `true`, all other values in `ActionSource` tied to the browser (`url`,
  // `incognito`, etc.) should be ignored since those properties only apply to
  // Chrome tabs. This field is only used for clipboard interactions, and as
  // such defaults to "false".
  bool os_clipboard = false;

  // Indicates that the source of the data is a separate Chrome profile from the
  // one receiving that data. This field is only used for clipboard
  // interactions, and as such defaults to "false".
  bool other_profile = false;
};

struct ActionDestination {
  // Returns true if all fields are set to default values.
  bool empty() const;

  GURL url;
  bool incognito = false;

  // Indicates that the destination of the data is the OS clipboard. While it's
  // not possible to know if the user intends to paste the data they copied in
  // Chrome or outside of it through the OS clipboard, this field can be used to
  // determine which rule trigger and what UX might be shown to the user
  // (blocking diallog vs string replacement in the clipboard).
  //
  // If this is `true`, all other values in `ActionDestination` tied to the
  // browser (`url`, `incognito`, etc.) should be ignored since those properties
  // only apply to Chrome tabs. This field is only used for clipboard
  // interactions, and as such defaults to "false".
  bool os_clipboard = false;

  // Indicates that the destination of the data is a separate Chrome profile
  // from the one providing that data. As such, this should only be set to
  // "true" for tab-to-tab clipboard interactions, destinations like the OS
  // clipboard are not considered a separate profile. This field is only used
  // for clipboard interactions, and as such defaults to "false".
  bool other_profile = false;

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

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_ACTION_CONTEXT_H_
