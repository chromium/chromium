// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"

SideSearchSideContentsHelper::~SideSearchSideContentsHelper() = default;

SideSearchSideContentsHelper::SideSearchSideContentsHelper(
    content::WebContents* web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SideSearchSideContentsHelper)
