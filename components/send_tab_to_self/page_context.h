// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_PAGE_CONTEXT_H_
#define COMPONENTS_SEND_TAB_TO_SELF_PAGE_CONTEXT_H_

namespace send_tab_to_self {

struct PageContext {
  PageContext();
  PageContext(const PageContext&);
  PageContext(PageContext&&);
  PageContext& operator=(const PageContext&);
  PageContext& operator=(PageContext&&);
  ~PageContext();
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_PAGE_CONTEXT_H_
