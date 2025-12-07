// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_ITERATOR_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_ITERATOR_H_

#include "base/functional/function_ref.h"

namespace tabs {

class TabInterface;

// Iterates through all TabInterfaces across all Profiles and Browsers. If
// `on_tab` returns false iteration is stopped.
void ForEachTabInterface(base::FunctionRef<bool(tabs::TabInterface*)> on_tab);

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_ITERATOR_H_
