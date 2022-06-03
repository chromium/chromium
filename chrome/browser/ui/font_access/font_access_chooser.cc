// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/font_access/font_access_chooser.h"
#include "content/public/browser/font_access_chooser.h"

FontAccessChooser::FontAccessChooser(base::OnceClosure close_callback)
    : closure_runner_(std::move(close_callback)) {}
