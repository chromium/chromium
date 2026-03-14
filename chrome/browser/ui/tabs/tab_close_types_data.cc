// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_close_types_data.h"

#include "content/public/browser/web_contents.h"

TabCloseTypesData::TabCloseTypesData(content::WebContents* web_contents,
                                     uint32_t close_types)
    : content::WebContentsUserData<TabCloseTypesData>(*web_contents),
      close_types_(close_types) {}

TabCloseTypesData::~TabCloseTypesData() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabCloseTypesData);
