// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"

#include "chrome/browser/ui/tabs/tab_model.h"

TabStripModelDelegate::NewStripContents::NewStripContents() = default;
TabStripModelDelegate::NewStripContents::~NewStripContents() = default;
TabStripModelDelegate::NewStripContents::NewStripContents(NewStripContents&&) =
    default;
